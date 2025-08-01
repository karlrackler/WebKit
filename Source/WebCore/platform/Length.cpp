/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Length.h"

#include "AnimationUtilities.h"
#include "CalculationCategory.h"
#include "CalculationTree.h"
#include "CalculationValue.h"
#include "CalculationValueMap.h"
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Length);

struct SameSizeAsLength {
    int32_t value;
    int32_t metaData;
};
static_assert(sizeof(Length) == sizeof(SameSizeAsLength), "length should stay small");

static Length parseLength(std::span<const char16_t> data)
{
    if (data.empty())
        return Length(1, LengthType::Relative);

    unsigned i = 0;
    while (i < data.size() && deprecatedIsSpaceOrNewline(data[i]))
        ++i;
    if (i < data.size() && (data[i] == '+' || data[i] == '-'))
        ++i;
    while (i < data.size() && isASCIIDigit(data[i]))
        ++i;
    unsigned intLength = i;
    while (i < data.size() && (isASCIIDigit(data[i]) || data[i] == '.'))
        ++i;
    unsigned doubleLength = i;

    // IE quirk: Skip whitespace between the number and the % character (20 % => 20%).
    while (i < data.size() && deprecatedIsSpaceOrNewline(data[i]))
        ++i;

    bool ok;
    char16_t next = (i < data.size()) ? data[i] : ' ';
    if (next == '%') {
        // IE quirk: accept decimal fractions for percentages.
        double r = charactersToDouble(data.first(doubleLength), &ok);
        if (ok)
            return Length(r, LengthType::Percent);
        return Length(1, LengthType::Relative);
    }
    auto r = parseInteger<int>(data.first(intLength));
    if (next == '*')
        return Length(r.value_or(1), LengthType::Relative);
    if (r)
        return Length(*r, LengthType::Fixed);
    return Length(0, LengthType::Relative);
}

static unsigned countCharacter(StringImpl& string, char16_t character)
{
    unsigned count = 0;
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i)
        count += string[i] == character;
    return count;
}

UniqueArray<Length> newLengthArray(const String& string, int& len)
{
    RefPtr<StringImpl> str = string.impl()->simplifyWhiteSpace(deprecatedIsSpaceOrNewline);
    if (!str->length()) {
        len = 1;
        return nullptr;
    }

    len = countCharacter(*str, ',') + 1;
    auto r = makeUniqueArray<Length>(len);

    int i = 0;
    unsigned pos = 0;
    size_t pos2;

    auto upconvertedCharacters = StringView(str.get()).upconvertedCharacters();
    while ((pos2 = str->find(',', pos)) != notFound) {
        r[i++] = parseLength(upconvertedCharacters.span().subspan(pos, pos2 - pos));
        pos = pos2+1;
    }

    ASSERT(i == len - 1);

    // IE Quirk: If the last comma is the last char skip it and reduce len by one.
    if (str->length() - pos > 0)
        r[i] = parseLength(upconvertedCharacters.span().subspan(pos));
    else
        len--;

    return r;
}

Length::Length(Ref<CalculationValue>&& value)
    : m_type(LengthType::Calculated)
{
    m_calculationValueHandle = CalculationValueMap::calculationValues().insert(WTFMove(value));
}

CalculationValue& Length::calculationValue() const
{
    ASSERT(isCalculated());
    return CalculationValueMap::calculationValues().get(m_calculationValueHandle);
}

Ref<CalculationValue> Length::protectedCalculationValue() const
{
    return calculationValue();
}
    
void Length::ref() const
{
    ASSERT(isCalculated());
    CalculationValueMap::calculationValues().ref(m_calculationValueHandle);
}

void Length::deref() const
{
    ASSERT(isCalculated());
    CalculationValueMap::calculationValues().deref(m_calculationValueHandle);
}

LengthType Length::typeFromIndex(const IPCData& data)
{
    static_assert(WTF::VariantSizeV<IPCData> == 13);
    switch (data.index()) {
    case WTF::alternativeIndexV<AutoData, IPCData>:
        return LengthType::Auto;
    case WTF::alternativeIndexV<NormalData, IPCData>:
        return LengthType::Normal;
    case WTF::alternativeIndexV<RelativeData, IPCData>:
        return LengthType::Relative;
    case WTF::alternativeIndexV<PercentData, IPCData>:
        return LengthType::Percent;
    case WTF::alternativeIndexV<FixedData, IPCData>:
        return LengthType::Fixed;
    case WTF::alternativeIndexV<IntrinsicData, IPCData>:
        return LengthType::Intrinsic;
    case WTF::alternativeIndexV<MinIntrinsicData, IPCData>:
        return LengthType::MinIntrinsic;
    case WTF::alternativeIndexV<MinContentData, IPCData>:
        return LengthType::MinContent;
    case WTF::alternativeIndexV<MaxContentData, IPCData>:
        return LengthType::MaxContent;
    case WTF::alternativeIndexV<FillAvailableData, IPCData>:
        return LengthType::FillAvailable;
    case WTF::alternativeIndexV<FitContentData, IPCData>:
        return LengthType::FitContent;
    case WTF::alternativeIndexV<ContentData, IPCData>:
        return LengthType::Content;
    case WTF::alternativeIndexV<UndefinedData, IPCData>:
        return LengthType::Undefined;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Length::Length(IPCData&& data)
    : m_type(typeFromIndex(data))
{
    WTF::switchOn(data,
        [&] (auto data) {
            WTF::switchOn(data.value,
                [&] (float value) {
                    m_isFloat = true;
                    m_floatValue = value;
                },
                [&] (int value) {
                    m_isFloat = false;
                    m_intValue = value;
                }
            );
            m_hasQuirk = data.hasQuirk;
        },
        []<typename EmptyData> (EmptyData) requires std::is_empty_v<EmptyData> { }
    );
}

auto Length::ipcData() const -> IPCData
{
    switch (m_type) {
    case LengthType::Auto:
        return AutoData { };
    case LengthType::Normal:
        return NormalData { };
    case LengthType::Relative:
        return RelativeData { floatOrInt(), m_hasQuirk };
    case LengthType::Percent:
        return PercentData { floatOrInt(), m_hasQuirk };
    case LengthType::Fixed:
        return FixedData { floatOrInt(), m_hasQuirk };
    case LengthType::Intrinsic:
        return IntrinsicData { floatOrInt(), m_hasQuirk };
    case LengthType::MinIntrinsic:
        return MinIntrinsicData { floatOrInt(), m_hasQuirk };
    case LengthType::MinContent:
        return MinContentData { floatOrInt(), m_hasQuirk };
    case LengthType::MaxContent:
        return MaxContentData { floatOrInt(), m_hasQuirk };
    case LengthType::FillAvailable:
        return FillAvailableData { floatOrInt(), m_hasQuirk };
    case LengthType::FitContent:
        return FitContentData { floatOrInt(), m_hasQuirk };
    case LengthType::Content:
        return ContentData { };
    case LengthType::Undefined:
        return UndefinedData { };
    case LengthType::Calculated:
        ASSERT_NOT_REACHED();
        return { };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto Length::floatOrInt() const -> FloatOrInt
{
    ASSERT(!isCalculated());
    if (m_isFloat)
        return m_floatValue;
    return m_intValue;
}

float Length::nonNanCalculatedValue(float maxValue) const
{
    ASSERT(isCalculated());
    float result = protectedCalculationValue()->evaluate(maxValue);
    if (std::isnan(result))
        return 0;
    return result;
}

bool Length::isCalculatedEqual(const Length& other) const
{
    return calculationValue() == other.calculationValue();
}

static Calculation::Child lengthCalculation(const Length& length)
{
    if (length.isPercent())
        return Calculation::percentage(length.value());

    if (length.isCalculated())
        return length.calculationValue().copyRoot();

    ASSERT(length.isFixed());
    return Calculation::dimension(length.value());
}

static Length makeLength(Calculation::Child&& root)
{
    // FIXME: Value range should be passed in.

    // NOTE: category is always `LengthPercentage` as late resolved `Length` values defined by percentages is the only reason calculation value is needed by `Length`.
    return Length(CalculationValue::create(Calculation::Category::LengthPercentage, Calculation::All, Calculation::Tree { WTFMove(root) }));
}

Length convertTo100PercentMinusLength(const Length& length)
{
    // If `length` is 0 or a percentage, we can avoid the `calc` altogether.
    if (length.isZero() || length.isPercent())
        return Length(100 - length.value(), LengthType::Percent);

    // Otherwise, turn this into a calc expression: calc(100% - length)
    return makeLength(Calculation::subtract(Calculation::percentage(100), lengthCalculation(length)));
}

Length convertTo100PercentMinusLengthSum(const Length& a, const Length& b)
{
    // If both `a` and `b` are 0, turn this into a calc expression: calc(100% - (0 + 0)) aka `100%`.
    if (a.isZero() && b.isZero())
        return Length(100, LengthType::Percent);

    // If just `a` is 0, we can just consider the case of `calc(100% - b)`.
    if (a.isZero()) {
        // And if `b` is a percent, we can avoid the `calc` altogether.
        if (b.isPercent())
            return Length(100 - b.value(), LengthType::Percent);
        return makeLength(Calculation::subtract(Calculation::percentage(100), lengthCalculation(b)));
    }

    // If just `b` is 0, we can just consider the case of `calc(100% - a)`.
    if (b.isZero()) {
        // And if `a` is a percent, we can avoid the `calc` altogether.
        if (a.isPercent())
            return Length(100 - a.value(), LengthType::Percent);
        return makeLength(Calculation::subtract(Calculation::percentage(100), lengthCalculation(a)));
    }

    // If both and `a` and `b` are percentages, we can avoid the `calc` altogether.
    if (a.isPercent() && b.isPercent())
        return Length(100 - (a.value() + b.value()), LengthType::Percent);

    // Otherwise, turn this into a calc expression: calc(100% - (a + b))
    return makeLength(Calculation::subtract(Calculation::percentage(100), Calculation::add(lengthCalculation(a), lengthCalculation(b))));
}

static Length blendMixedTypes(const Length& from, const Length& to, const BlendingContext& context)
{
    if (context.compositeOperation != CompositeOperation::Replace)
        return makeLength(Calculation::add(lengthCalculation(from), lengthCalculation(to)));

    if ((!from.isSpecified() && !from.isRelative()) || (!to.isSpecified() && !to.isRelative())) {
        ASSERT(context.isDiscrete);
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

    if (from.isRelative() || to.isRelative())
        return { 0, LengthType::Fixed };

    if (!to.isCalculated() && !from.isPercent() && (context.progress == 1 || from.isZero()))
        return blend(Length(0, to.type()), to, context);

    if (!from.isCalculated() && !to.isPercent() && (!context.progress || to.isZero()))
        return blend(from, Length(0, from.type()), context);

    return makeLength(Calculation::blend(lengthCalculation(from), lengthCalculation(to), context.progress));
}

Length blend(const Length& from, const Length& to, const BlendingContext& context)
{
    if (from.isAuto() || to.isAuto() || from.isUndefined() || to.isUndefined() || from.isNormal() || to.isNormal())
        return context.progress < 0.5 ? from : to;

    if (from.isCalculated() || to.isCalculated() || (from.type() != to.type()))
        return blendMixedTypes(from, to, context);

    if (!context.progress && context.isReplace())
        return from;

    if (context.progress == 1 && context.isReplace())
        return to;

    LengthType resultType = to.type();
    if (to.isZero())
        resultType = from.type();

    if (resultType == LengthType::Percent) {
        float fromPercent = from.isZero() ? 0 : from.percent();
        float toPercent = to.isZero() ? 0 : to.percent();
        return Length(WebCore::blend(fromPercent, toPercent, context), LengthType::Percent);
    }

    float fromValue = from.isZero() ? 0 : from.value();
    float toValue = to.isZero() ? 0 : to.value();
    return Length(WebCore::blend(fromValue, toValue, context), resultType);
}

Length blend(const Length& from, const Length& to, const BlendingContext& context, ValueRange valueRange)
{
    auto blended = blend(from, to, context);
    if (valueRange == ValueRange::NonNegative && blended.isNegative()) {
        auto type = from.isZero() ? to.type() : from.type();
        if (type != LengthType::Calculated)
            return { 0, type };
        return { 0, LengthType::Fixed };
    }
    return blended;
}

static TextStream& operator<<(TextStream& ts, LengthType type)
{
    switch (type) {
    case LengthType::Auto: ts << "auto"_s; break;
    case LengthType::Calculated: ts << "calc"_s; break;
    case LengthType::Content: ts << "content"_s; break;
    case LengthType::FillAvailable: ts << "fill-available"_s; break;
    case LengthType::FitContent: ts << "fit-content"_s; break;
    case LengthType::Fixed: ts << "fixed"_s; break;
    case LengthType::Intrinsic: ts << "intrinsic"_s; break;
    case LengthType::MinIntrinsic: ts << "min-intrinsic"_s; break;
    case LengthType::MinContent: ts << "min-content"_s; break;
    case LengthType::MaxContent: ts << "max-content"_s; break;
    case LengthType::Normal: ts << "normal"_s; break;
    case LengthType::Percent: ts << "percent"_s; break;
    case LengthType::Relative: ts << "relative"_s; break;
    case LengthType::Undefined: ts << "undefined"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, Length length)
{
    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Normal:
    case LengthType::Undefined:
        ts << length.type();
        break;
    case LengthType::Fixed:
        ts << TextStream::FormatNumberRespectingIntegers(length.value()) << "px"_s;
        break;
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
        ts << length.type() << ' ' << TextStream::FormatNumberRespectingIntegers(length.value());
        break;
    case LengthType::Percent:
        ts << TextStream::FormatNumberRespectingIntegers(length.percent()) << '%';
        break;
    case LengthType::Calculated:
        ts << length.protectedCalculationValue();
        break;
    }
    
    if (length.hasQuirk())
        ts << " has-quirk"_s;

    return ts;
}

} // namespace WebCore
