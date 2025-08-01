/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MathMLPresentationElement.h"

#if ENABLE(MATHML)

#include "CommonAtomStrings.h"
#include "ContainerNodeInlines.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLHtmlElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTTPParsers.h"
#include "MathMLMathElement.h"
#include "MathMLNames.h"
#include "RenderMathMLBlockInlines.h"
#include "RenderTableCell.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MathMLPresentationElement);

using namespace MathMLNames;

MathMLPresentationElement::MathMLPresentationElement(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> constructionType)
    : MathMLElement(tagName, document, constructionType)
{
}

Ref<MathMLPresentationElement> MathMLPresentationElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLPresentationElement(tagName, document));
}

RenderPtr<RenderElement> MathMLPresentationElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (hasTagName(mtableTag))
        return createRenderer<RenderMathMLTable>(*this, WTFMove(style));

    return MathMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

const MathMLElement::BooleanValue& MathMLPresentationElement::cachedBooleanAttribute(const QualifiedName& name, std::optional<BooleanValue>& attribute)
{
    if (attribute)
        return attribute.value();

    // In MathML, attribute values are case-sensitive.
    const AtomString& value = attributeWithoutSynchronization(name);
    if (value == trueAtom())
        attribute = BooleanValue::True;
    else if (value == falseAtom())
        attribute = BooleanValue::False;
    else
        attribute = BooleanValue::Default;

    return attribute.value();
}

MathMLElement::Length MathMLPresentationElement::parseNumberAndUnit(StringView string, bool acceptLegacyMathMLLengths)
{
    LengthType lengthType = LengthType::UnitLess;
    unsigned stringLength = string.length();
    char16_t lastChar = string[stringLength - 1];
    if (lastChar == '%') {
        lengthType = LengthType::Percentage;
        stringLength--;
    } else if (stringLength >= 2) {
        char16_t penultimateChar = string[stringLength - 2];
        if (penultimateChar == 'c' && lastChar == 'm')
            lengthType = LengthType::Cm;
        if (penultimateChar == 'e' && lastChar == 'm')
            lengthType = LengthType::Em;
        else if (penultimateChar == 'e' && lastChar == 'x')
            lengthType = LengthType::Ex;
        else if (penultimateChar == 'i' && lastChar == 'n')
            lengthType = LengthType::In;
        else if (penultimateChar == 'm' && lastChar == 'm')
            lengthType = LengthType::Mm;
        else if (penultimateChar == 'p' && lastChar == 'c')
            lengthType = LengthType::Pc;
        else if (penultimateChar == 'p' && lastChar == 't')
            lengthType = LengthType::Pt;
        else if (penultimateChar == 'p' && lastChar == 'x')
            lengthType = LengthType::Px;

        if (lengthType != LengthType::UnitLess)
            stringLength -= 2;
    }

    bool ok;
    float lengthValue = string.left(stringLength).toFloat(ok);
    if (!ok)
        return Length();

    if (!acceptLegacyMathMLLengths && lengthType == LengthType::UnitLess && string != "0"_s)
        return Length();

    Length length;
    length.type = lengthType;
    length.value = lengthValue;
    return length;
}

MathMLElement::Length MathMLPresentationElement::parseNamedSpace(StringView string)
{
    // Named space values are case-sensitive.
    int namedSpaceValue;
    if (string == "veryverythinmathspace"_s)
        namedSpaceValue = 1;
    else if (string == "verythinmathspace"_s)
        namedSpaceValue = 2;
    else if (string == "thinmathspace"_s)
        namedSpaceValue = 3;
    else if (string == "mediummathspace"_s)
        namedSpaceValue = 4;
    else if (string == "thickmathspace"_s)
        namedSpaceValue = 5;
    else if (string == "verythickmathspace"_s)
        namedSpaceValue = 6;
    else if (string == "veryverythickmathspace"_s)
        namedSpaceValue = 7;
    else if (string == "negativeveryverythinmathspace"_s)
        namedSpaceValue = -1;
    else if (string == "negativeverythinmathspace"_s)
        namedSpaceValue = -2;
    else if (string == "negativethinmathspace"_s)
        namedSpaceValue = -3;
    else if (string == "negativemediummathspace"_s)
        namedSpaceValue = -4;
    else if (string == "negativethickmathspace"_s)
        namedSpaceValue = -5;
    else if (string == "negativeverythickmathspace"_s)
        namedSpaceValue = -6;
    else if (string == "negativeveryverythickmathspace"_s)
        namedSpaceValue = -7;
    else
        return Length();

    Length length;
    length.type = LengthType::MathUnit;
    length.value = namedSpaceValue;
    return length;
}

MathMLElement::Length MathMLPresentationElement::parseMathMLLength(const String& string, bool acceptLegacyMathMLLengths)
{
    // The regular expression from the MathML Relax NG schema is as follows:
    //
    //   pattern = '\s*((-?[0-9]*([0-9]\.?|\.[0-9])[0-9]*(e[mx]|in|cm|mm|p[xtc]|%)?)|(negative)?((very){0,2}thi(n|ck)|medium)mathspace)\s*'
    //
    // We do not perform a strict verification of the syntax of whitespaces and number.
    // Instead, we just use isASCIIWhitespace and toFloat to parse these parts.

    // We first skip whitespace from both ends of the string.
    StringView stringView = string;
    StringView trimmedLength = stringView.trim(isASCIIWhitespaceWithoutFF<char16_t>);

    if (trimmedLength.isEmpty())
        return Length();

    // We consider the most typical case: a number followed by an optional unit.
    char16_t firstChar = trimmedLength[0];
    if (isASCIIDigit(firstChar) || firstChar == '-' || firstChar == '.')
        return parseNumberAndUnit(trimmedLength, acceptLegacyMathMLLengths);

    // Otherwise, we try and parse a named space.
    if (!acceptLegacyMathMLLengths)
        return Length();
    return parseNamedSpace(trimmedLength);
}

const MathMLElement::Length& MathMLPresentationElement::cachedMathMLLength(const QualifiedName& name, std::optional<Length>& length)
{
    if (length)
        return length.value();
    length = parseMathMLLength(attributeWithoutSynchronization(name), !document().settings().coreMathMLEnabled());
    return length.value();
}

MathMLElement::MathVariant MathMLPresentationElement::parseMathVariantAttribute(const AtomString& attributeValue)
{
    // The mathvariant attribute values is case-sensitive.
    static constexpr std::pair<ComparableASCIILiteral, MathVariant> mappings[] = {
        { "bold"_s, MathVariant::Bold },
        { "bold-fraktur"_s, MathVariant::BoldFraktur },
        { "bold-italic"_s, MathVariant::BoldItalic },
        { "bold-sans-serif"_s, MathVariant::BoldSansSerif },
        { "bold-script"_s, MathVariant::BoldScript },
        { "double-struck"_s, MathVariant::DoubleStruck },
        { "fraktur"_s, MathVariant::Fraktur },
        { "initial"_s, MathVariant::Initial },
        { "italic"_s, MathVariant::Italic },
        { "looped"_s, MathVariant::Looped },
        { "monospace"_s, MathVariant::Monospace },
        { "normal"_s, MathVariant::Normal },
        { "sans-serif"_s, MathVariant::SansSerif },
        { "sans-serif-bold-italic"_s, MathVariant::SansSerifBoldItalic },
        { "sans-serif-italic"_s, MathVariant::SansSerifItalic },
        { "script"_s, MathVariant::Script },
        { "stretched"_s, MathVariant::Stretched },
        { "tailed"_s, MathVariant::Tailed },
    };
    static constexpr SortedArrayMap map { mappings };
    return map.get(attributeValue, MathVariant::None);
}

std::optional<MathMLElement::MathVariant> MathMLPresentationElement::specifiedMathVariant()
{
    if (!acceptsMathVariantAttribute())
        return std::nullopt;
    if (!m_mathVariant)
        m_mathVariant = parseMathVariantAttribute(attributeWithoutSynchronization(mathvariantAttr));
    return m_mathVariant.value() == MathVariant::None ? std::nullopt : m_mathVariant;
}

void MathMLPresentationElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == mathvariantAttr && acceptsMathVariantAttribute()) {
        m_mathVariant = std::nullopt;
        if (renderer())
            MathMLStyle::resolveMathMLStyleTree(renderer());
    }

    MathMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

}

#endif // ENABLE(MATHML)
