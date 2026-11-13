#include "native/Engine/DocumentXmlPreflight.h"
#include <iostream>
#include <string>
#include <string_view>

using dust3d::native::DocumentXmlError;
using dust3d::native::inspectDocumentXmlEnvelope;

int main()
{
    int failures = 0;
    auto require = [&](std::string_view xml, DocumentXmlError expected, const char* description) {
        if (inspectDocumentXmlEnvelope(xml) != expected) {
            std::cerr << description << '\n';
            ++failures;
        }
    };
    require(R"(<canvas name="&#65; &#x1F600;">&#x10FFFF;</canvas>)", DocumentXmlError::None,
        "Valid decimal and hexadecimal references must be admitted");
    require(R"(<canvas name="&amp;#0; &legacy;">&#00000065;</canvas>)", DocumentXmlError::None,
        "Escaped numeric text, leading zeros and legacy named literals must be preserved");
    require(R"(<?metadata &#0;?><canvas><!-- &#0; --><![CDATA[&#xD800;]]></canvas>)", DocumentXmlError::None,
        "Numeric-looking text in processing instructions, comments and CDATA must remain literal");
    require(R"(<canvas name="&#0;"/>)", DocumentXmlError::InvalidNumericReference,
        "An attribute must reject the NUL reference");
    require(R"(<canvas>&#xD800;</canvas>)", DocumentXmlError::InvalidNumericReference,
        "Element text must reject a surrogate reference");
    require(R"(<canvas name="&#x110000;"/>)", DocumentXmlError::InvalidNumericReference,
        "A reference beyond the Unicode maximum must be rejected before decoding");
    require(R"(<canvas name="&#12A;"/>)", DocumentXmlError::InvalidNumericReference,
        "A decimal reference must reject hexadecimal letters");
    require(R"(<canvas name="&#x;"/>)", DocumentXmlError::InvalidNumericReference,
        "An empty hexadecimal reference must be rejected");
    require(R"(<canvas>&#32</canvas>)", DocumentXmlError::InvalidNumericReference,
        "A numeric reference must end with a semicolon");
    require(R"(<canvas name="<!-- &#0; -->"/>)", DocumentXmlError::InvalidNumericReference,
        "A comment-looking marker inside a quoted attribute must not hide the reference");
    require(R"(<canvas name="&#9;&#10;&#13;&#xD7FF;&#xE000;&#xFFFD;"/>)", DocumentXmlError::None,
        "Legal whitespace and XML character range boundaries must be admitted");
    require(R"(<canvas name="&#xFFFE;"/>)", DocumentXmlError::InvalidNumericReference,
        "The XML noncharacter U+FFFE must be rejected");
    require(R"(<!DOCTYPE canvas><canvas/>)", DocumentXmlError::UnsupportedDeclaration,
        "The existing DTD restriction must remain in place");
    std::string deepest;
    for (int level = 0; level < 128; ++level)
        deepest += "<node>";
    for (int level = 0; level < 128; ++level)
        deepest += "</node>";
    require(deepest, DocumentXmlError::None, "The existing nesting boundary must remain valid");
    require("<node>" + deepest + "</node>", DocumentXmlError::DepthLimit,
        "Exceeding the existing depth limit must retain its typed rejection");
    return failures != 0;
}
