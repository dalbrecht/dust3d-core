#ifndef DUST3D_NATIVE_DOCUMENT_XML_PREFLIGHT_H_
#define DUST3D_NATIVE_DOCUMENT_XML_PREFLIGHT_H_

#include <cstdint>
#include <string_view>

namespace dust3d::native {

enum class DocumentXmlError {
    None,
    EmbeddedNul,
    UnterminatedSpecial,
    UnsupportedDeclaration,
    UnterminatedElement,
    Unbalanced,
    DepthLimit,
    InvalidNumericReference,
};

// Check only numeric references. RapidXML's legacy treatment of other text
// and unknown named references stays unchanged.
inline bool hasValidDocumentNumericReferences(std::string_view text)
{
    constexpr uint32_t maximumCodePoint = 0x10FFFF;
    for (auto start = text.find("&#"); start != std::string_view::npos; start = text.find("&#", start)) {
        start += 2;
        const bool hexadecimal = start < text.size() && text[start] == 'x';
        if (hexadecimal)
            ++start;
        const auto firstDigit = start;
        const uint32_t base = hexadecimal ? 16 : 10;
        uint32_t codePoint = 0;
        for (; start < text.size() && text[start] != ';'; ++start) {
            const auto character = text[start];
            uint32_t digit;
            if (character >= '0' && character <= '9')
                digit = character - '0';
            else if (hexadecimal && character >= 'a' && character <= 'f')
                digit = character - 'a' + 10;
            else if (hexadecimal && character >= 'A' && character <= 'F')
                digit = character - 'A' + 10;
            else
                return false;
            if (codePoint > (maximumCodePoint - digit) / base)
                return false;
            codePoint = codePoint * base + digit;
        }
        if (start == firstDigit || start == text.size())
            return false;
        const bool xmlCharacter = codePoint == 0x9 || codePoint == 0xA || codePoint == 0xD
            || (codePoint >= 0x20 && codePoint <= 0xD7FF)
            || (codePoint >= 0xE000 && codePoint <= 0xFFFD)
            || codePoint >= 0x10000;
        if (!xmlCharacter)
            return false;
        ++start;
    }
    return true;
}

// RapidXML is recursive. Bound depth before parsing so hostile XML cannot exhaust
// the stack. XML declarations/comments/CDATA do not increase depth.
inline DocumentXmlError inspectDocumentXmlEnvelope(std::string_view xml)
{
    if (xml.find('\0') != std::string_view::npos)
        return DocumentXmlError::EmbeddedNul;
    size_t depth = 0;
    for (size_t position = 0; position < xml.size();) {
        const auto start = xml.find('<', position);
        const auto length = start == std::string_view::npos ? std::string_view::npos : start - position;
        if (!hasValidDocumentNumericReferences(xml.substr(position, length)))
            return DocumentXmlError::InvalidNumericReference;
        if (start == std::string_view::npos)
            break;
        if (xml.substr(start, 4) == "<!--" || xml.substr(start, 9) == "<![CDATA[" || xml.substr(start, 2) == "<?") {
            const auto terminator = xml.substr(start, 4) == "<!--" ? "-->" : (xml.substr(start, 2) == "<?" ? "?>" : "]]>");
            const auto end = xml.find(terminator, start + 2);
            if (end == std::string_view::npos)
                return DocumentXmlError::UnterminatedSpecial;
            position = end + std::char_traits<char>::length(terminator);
            continue;
        }
        if (xml.substr(start, 2) == "<!")
            return DocumentXmlError::UnsupportedDeclaration;
        char quote = 0;
        size_t attributeStart = 0;
        auto end = start + 1;
        for (; end < xml.size(); ++end) {
            const char character = xml[end];
            if (quote) {
                if (character == quote) {
                    if (!hasValidDocumentNumericReferences(xml.substr(attributeStart, end - attributeStart)))
                        return DocumentXmlError::InvalidNumericReference;
                    quote = 0;
                }
            } else if (character == '\'' || character == '"') {
                quote = character;
                attributeStart = end + 1;
            } else if (character == '>') {
                break;
            }
        }
        if (end == xml.size())
            return DocumentXmlError::UnterminatedElement;
        if (xml[start + 1] == '/') {
            if (!depth)
                return DocumentXmlError::Unbalanced;
            --depth;
        } else if (xml[end - 1] != '/') {
            if (++depth > 128)
                return DocumentXmlError::DepthLimit;
        }
        position = end + 1;
    }
    if (depth)
        return DocumentXmlError::Unbalanced;
    return DocumentXmlError::None;
}

}

#endif
