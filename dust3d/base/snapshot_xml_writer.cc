/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

// SPDX-License-Identifier: MIT
#include <dust3d/base/snapshot_xml.h>
#include <set>
#include <string_view>
#include <vector>

namespace dust3d {
namespace {
    using Attributes = std::map<std::string, std::string>;
    using WriteErrorCode = SnapshotXmlWriteErrorCode;
    struct ComponentEvent {
        const Attributes* attributes;
        size_t depth;
        bool isClosing;
    };
    struct ComponentFrame {
        const Attributes* attributes;
        std::string_view children;
        size_t cursor;
        size_t depth;
        bool isOpened;
    };
    struct ComponentPlan {
        std::vector<ComponentEvent> events;
        bool hasRootChildren;
    };
    void chargeBytes(size_t& used, size_t count, size_t limit)
    {
        if (count > limit - used)
            throw SnapshotXmlWriteError(WriteErrorCode::limitExceeded, "Snapshot XML write exceeds its byte budget");
        used += count;
    }
    void validateLimits(const SnapshotXmlWriteLimits& limits)
    {
        if (!limits.maximumComponents || limits.maximumComponents > 65536
            || !limits.maximumDepth || limits.maximumDepth > 126
            || !limits.maximumXmlBytes || limits.maximumXmlBytes > 16 * 1024 * 1024)
            throw SnapshotXmlWriteError(WriteErrorCode::invalidLimits, "Snapshot XML limits must tighten the documented hard ceilings");
    }
    std::string_view findComponentChildren(const Attributes& attributes)
    {
        auto found = attributes.find("children");
        return found == attributes.end() ? std::string_view {} : std::string_view(found->second);
    }
    ComponentPlan planComponents(const Snapshot& snapshot, const SnapshotXmlWriteLimits& limits)
    {
        validateLimits(limits);
        if (snapshot.components.size() > limits.maximumComponents)
            throw SnapshotXmlWriteError(WriteErrorCode::limitExceeded, "Snapshot XML component count exceeds its budget");
        ComponentPlan plan { {}, snapshot.rootComponent.count("children") != 0 };
        const auto roots = findComponentChildren(snapshot.rootComponent);
        size_t referenceBytes = 0;
        chargeBytes(referenceBytes, roots.size(), limits.maximumXmlBytes);
        std::vector<ComponentFrame> stack { { nullptr, roots, 0, 0, true } };
        std::set<const Attributes*> visited;
        while (!stack.empty()) {
            auto& frame = stack.back();
            if (!frame.isOpened) {
                plan.events.push_back({ frame.attributes, frame.depth, false });
                frame.isOpened = true;
            }
            if (frame.cursor > frame.children.size()) {
                if (frame.attributes)
                    plan.events.push_back({ frame.attributes, frame.depth, true });
                stack.pop_back();
                continue;
            }
            const auto end = frame.children.find(',', frame.cursor);
            const auto child = frame.children.substr(frame.cursor, end == std::string_view::npos ? end : end - frame.cursor);
            frame.cursor = end == std::string_view::npos ? frame.children.size() + 1 : end + 1;
            if (child.empty())
                continue;
            auto found = snapshot.components.find(std::string(child));
            if (found == snapshot.components.end() || !visited.insert(&found->second).second)
                throw SnapshotXmlWriteError(WriteErrorCode::invalidHierarchy, "Snapshot XML hierarchy has a missing or repeated component");
            const size_t depth = stack.size() - 1;
            if (depth >= limits.maximumDepth)
                throw SnapshotXmlWriteError(WriteErrorCode::limitExceeded, "Snapshot XML component depth exceeds its budget");
            const auto children = findComponentChildren(found->second);
            chargeBytes(referenceBytes, children.size(), limits.maximumXmlBytes);
            stack.push_back({ &found->second, children, 0, depth, false });
        }
        if (visited.size() != snapshot.components.size())
            throw SnapshotXmlWriteError(WriteErrorCode::invalidHierarchy, "Snapshot XML hierarchy would omit detached components");
        return plan;
    }
    class XmlSink {
    public:
        XmlSink(size_t limit, std::string* output = nullptr)
            : m_limit(limit)
            , m_output(output)
        {
        }
        void append(std::string_view text)
        {
            chargeBytes(m_used, text.size(), m_limit);
            if (m_output)
                m_output->append(text.data(), text.size());
        }
        void appendEscaped(std::string_view text)
        {
            for (char character : text) {
                switch (character) {
                case '&':
                    append("&amp;");
                    break;
                case '<':
                    append("&lt;");
                    break;
                case '>':
                    append("&gt;");
                    break;
                case '"':
                    append("&quot;");
                    break;
                default:
                    append(std::string_view(&character, 1));
                    break;
                }
            }
        }
        size_t size() const { return m_used; }

    private:
        size_t m_limit;
        std::string* m_output;
        size_t m_used = 0;
    };
    void writeAttributes(XmlSink& sink, const Attributes& values, bool skipsInternal, bool skipsChildren = false)
    {
        for (const auto& attribute : values) {
            if ((skipsInternal && attribute.first.compare(0, 2, "__") == 0)
                || (skipsChildren && attribute.first == "children"))
                continue;
            sink.append(" ");
            sink.append(attribute.first);
            sink.append("=\"");
            sink.appendEscaped(attribute.second);
            sink.append("\"");
        }
    }
    void writeFlatElements(XmlSink& sink, const std::map<std::string, Attributes>& values,
        std::string_view name, bool skipsInternal)
    {
        for (const auto& value : values) {
            sink.append("  <");
            sink.append(name);
            writeAttributes(sink, value.second, skipsInternal);
            sink.append("/>\n");
        }
    }
    void writeXml(XmlSink& sink, const Snapshot& snapshot, const ComponentPlan& plan)
    {
        sink.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<canvas");
        writeAttributes(sink, snapshot.canvas, false);
        sink.append(">\n <nodes>\n");
        writeFlatElements(sink, snapshot.nodes, "node", false);
        sink.append(" </nodes>\n <edges>\n");
        writeFlatElements(sink, snapshot.edges, "edge", false);
        sink.append(" </edges>\n <parts>\n");
        writeFlatElements(sink, snapshot.parts, "part", true);
        sink.append(" </parts>\n");
        if (plan.hasRootChildren) {
            sink.append(" <components>\n");
            for (const auto& event : plan.events) {
                for (size_t index = 0; index < event.depth; ++index)
                    sink.append(" ");
                if (event.isClosing) {
                    sink.append("  </component>\n");
                } else {
                    sink.append("  <component");
                    writeAttributes(sink, *event.attributes, true, true);
                    sink.append(">\n");
                }
            }
            sink.append(" </components>\n");
        }
        sink.append(" <animations>\n");
        writeFlatElements(sink, snapshot.animations, "animation", false);
        sink.append(" </animations>\n</canvas>\n");
    }
}
void validateSnapshotForXmlWrite(const Snapshot& snapshot, const SnapshotXmlWriteLimits& limits)
{
    const auto plan = planComponents(snapshot, limits);
    XmlSink counter(limits.maximumXmlBytes);
    writeXml(counter, snapshot, plan);
}
void saveSnapshotToXmlString(const Snapshot& snapshot, std::string& xmlString)
{
    saveSnapshotToXmlString(snapshot, xmlString, {});
}
void saveSnapshotToXmlString(const Snapshot& snapshot, std::string& xmlString, const SnapshotXmlWriteLimits& limits)
{
    const auto plan = planComponents(snapshot, limits);
    if (xmlString.size() > limits.maximumXmlBytes)
        throw SnapshotXmlWriteError(WriteErrorCode::limitExceeded, "Existing snapshot XML prefix exceeds its byte budget");
    const size_t remaining = limits.maximumXmlBytes - xmlString.size();
    XmlSink counter(remaining);
    writeXml(counter, snapshot, plan);
    std::string staged;
    staged.reserve(counter.size());
    XmlSink output(remaining, &staged);
    writeXml(output, snapshot, plan);
    // Standard string append has a strong exception guarantee; publish only the complete staged result.
    xmlString.append(staged);
}
}
