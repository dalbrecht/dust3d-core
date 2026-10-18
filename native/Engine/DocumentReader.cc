#include "EngineInternal.h"
#include "ImportedModels.h"
#include <algorithm>
#include <charconv>
#include <dust3d/base/ds3_file.h>
#include <dust3d/base/snapshot_xml.h>
#include <rapidxml.hpp>
#include <set>
#include <sstream>
#include <string_view>

namespace dust3d {
namespace native {

    namespace {
        constexpr size_t maximumHeaderSize = 1024 * 1024;
        constexpr size_t maximumModelSize = 16 * 1024 * 1024;

        size_t parseSize(const std::string& text)
        {
            size_t value = 0;
            auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            if (text.empty() || parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The document has an invalid archive offset or size.");
            return value;
        }

        std::string requiredAttribute(rapidxml::xml_node<>* node, const char* name)
        {
            auto* attribute = node->first_attribute(name);
            if (!attribute || !attribute->value_size())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An archive item is missing a required attribute.");
            return std::string(attribute->value(), attribute->value_size());
        }

        // RapidXML is recursive. Bound depth before parsing so hostile XML cannot exhaust
        // the stack. XML declarations/comments/CDATA do not increase depth.
        void validateXmlEnvelope(std::string_view xml)
        {
            if (xml.find('\0') != std::string_view::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Embedded NUL bytes are not allowed in document XML.");
            size_t depth = 0;
            for (size_t position = 0; position < xml.size();) {
                const auto start = xml.find('<', position);
                if (start == std::string_view::npos)
                    break;
                if (xml.substr(start, 4) == "<!--" || xml.substr(start, 9) == "<![CDATA[" || xml.substr(start, 2) == "<?") {
                    const auto terminator = xml.substr(start, 4) == "<!--" ? "-->" : (xml.substr(start, 2) == "<?" ? "?>" : "]]>");
                    const auto end = xml.find(terminator, start + 2);
                    if (end == std::string_view::npos)
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unterminated XML declaration or comment.");
                    position = end + std::char_traits<char>::length(terminator);
                    continue;
                }
                if (xml.substr(start, 2) == "<!")
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "DTD declarations are not supported in .ds3 documents.");
                char quote = 0;
                auto end = start + 1;
                for (; end < xml.size(); ++end) {
                    const char character = xml[end];
                    if (quote) {
                        if (character == quote)
                            quote = 0;
                    } else if (character == '\'' || character == '"') {
                        quote = character;
                    } else if (character == '>') {
                        break;
                    }
                }
                if (end == xml.size())
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unterminated XML element.");
                if (xml[start + 1] == '/') {
                    if (!depth)
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unbalanced XML elements.");
                    --depth;
                } else if (xml[end - 1] != '/') {
                    if (++depth > 128)
                        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The document exceeds the XML nesting limit (128).");
                }
                position = end + 1;
            }
            if (depth)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unbalanced XML elements.");
        }

        void validateModelXml(std::vector<uint8_t>& bytes)
        {
            validateXmlEnvelope(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            bytes.push_back(0);
            std::vector<uint8_t> validationBytes(bytes);
            rapidxml::xml_document<> xml;
            xml.parse<rapidxml::parse_validate_closing_tags>(reinterpret_cast<char*>(validationBytes.data()));
            auto* canvas = xml.first_node("canvas");
            if (!canvas || xml.first_node() != canvas || canvas->next_sibling())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The model must have a single canvas root element.");
            for (const auto* section : { "nodes", "edges", "parts" }) {
                std::set<std::string> ids;
                if (auto* elements = canvas->first_node(section)) {
                    for (auto* element = elements->first_node(); element; element = element->next_sibling()) {
                        if (!ids.insert(requiredAttribute(element, "id")).second)
                            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The model contains duplicate identifiers.");
                    }
                }
            }
            std::set<std::string> componentIds;
            std::vector<rapidxml::xml_node<>*> pending;
            if (auto* components = canvas->first_node("components"))
                pending.push_back(components);
            while (!pending.empty()) {
                auto* parent = pending.back();
                pending.pop_back();
                for (auto* component = parent->first_node(); component; component = component->next_sibling()) {
                    if (!componentIds.insert(requiredAttribute(component, "id")).second)
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The model contains duplicate component identifiers.");
                    pending.push_back(component);
                }
            }
        }
    }

    std::unique_ptr<Snapshot> readDocument(const uint8_t* bytes, size_t size, std::string& warning, DocumentAssetObserver* observer, ImportedModels* imports)
    {
        if (!bytes || !size)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Choose a nonempty .ds3 document.");
        if (size > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The document exceeds the 256 MiB file limit.");
        const auto* lineEnd = std::find(bytes, bytes + std::min(size, size_t(1024)), uint8_t('\n'));
        if (lineEnd == bytes + std::min(size, size_t(1024)))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The .ds3 header is missing or too long.");
        std::istringstream firstLine(std::string(reinterpret_cast<const char*>(bytes), lineEnd - bytes));
        std::string magic, version, format, offsetText, extra;
        if (!(firstLine >> magic >> version >> format >> offsetText) || firstLine >> extra
            || (magic != Ds3FileReader::m_magicApplicationName && magic != "DUST3D") || version != "1.0" || format != "xml")
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "This is not a supported Dust3D 1.0 document.");
        const size_t binaryOffset = parseSize(offsetText);
        const size_t firstLineSize = size_t(lineEnd - bytes) + 1;
        if (binaryOffset <= firstLineSize || binaryOffset > size)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The archive header offset is outside the document.");
        if (binaryOffset - firstLineSize > maximumHeaderSize)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The archive header exceeds the 1 MiB limit.");

        std::string header(reinterpret_cast<const char*>(bytes + firstLineSize), binaryOffset - firstLineSize);
        validateXmlEnvelope(header);
        rapidxml::xml_document<> xml;
        xml.parse<rapidxml::parse_validate_closing_tags>(header.data());
        auto* root = xml.first_node("ds3");
        if (!root || xml.first_node() != root || root->next_sibling())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The archive must have a single ds3 root element.");
        std::set<std::string> names;
        std::vector<ModelAssetView> modelAssets;
        size_t modelCount = 0;
        std::string modelName;
        for (auto* item = root->first_node(); item; item = item->next_sibling()) {
            const auto name = String::trimmed(requiredAttribute(item, "name"));
            const auto offset = parseSize(requiredAttribute(item, "offset"));
            const auto itemSize = parseSize(requiredAttribute(item, "size"));
            const auto payloadSize = size - binaryOffset;
            if (name.empty() || offset > payloadSize || itemSize > payloadSize - offset || !names.insert(name).second)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An archive item is duplicated or outside the document.");
            const std::string type(item->name(), item->name_size());
            if (type == "model") {
                ++modelCount;
                modelName = name;
                if (itemSize > maximumModelSize)
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The model XML exceeds the 16 MiB limit.");
            } else if (type == "asset" && name.compare(0, 7, "models/") == 0) {
                if (!imports)
                    throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "This operation does not yet support imported GLB materials and UVs.");
                modelAssets.push_back({ name, bytes + binaryOffset + offset, itemSize });
            } else if (type == "asset" && name.compare(0, 7, "images/") == 0) {
                warning = "Untextured preview: image textures are not yet displayed.";
            }
            if (observer && type != "model") {
                observer->item(type, name, bytes + binaryOffset + offset, itemSize);
                for (auto* attribute = item->first_attribute(); attribute; attribute = attribute->next_attribute()) {
                    const std::string_view key(attribute->name(), attribute->name_size());
                    if (key != "name" && key != "offset" && key != "size")
                        observer->attribute(key, std::string_view(attribute->value(), attribute->value_size()));
                }
            }
        }
        if (modelCount != 1)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The archive must contain exactly one model item.");

        // The inherited reader is only invoked after its unchecked archive arithmetic
        // and required attributes have been validated at this trust boundary.
        Ds3FileReader reader(bytes, size);
        std::vector<uint8_t> modelBytes;
        reader.loadItem(modelName, &modelBytes);
        validateModelXml(modelBytes);
        auto snapshot = std::make_unique<Snapshot>();
        loadSnapshotFromXmlString(snapshot.get(), reinterpret_cast<char*>(modelBytes.data()));
        validateSnapshot(*snapshot, imports != nullptr);
        for (const auto& component : snapshot->components) {
            if (!String::valueOrEmpty(component.second, "colorImageId").empty()
                || !String::valueOrEmpty(component.second, "wrapPattern").empty()
                || !String::valueOrEmpty(component.second, "wrapWrinkles").empty())
                warning = "Untextured preview: image textures and procedural surface details are not yet displayed.";
        }
        if (imports)
            imports->load(*snapshot, modelAssets, warning);
        return snapshot;
    }

}
}
