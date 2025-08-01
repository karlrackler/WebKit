/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IntlObject.h"

#include "Error.h"
#include "FunctionPrototype.h"
#include "GlobalObjectMethodTable.h"
#include "IntlCollator.h"
#include "IntlCollatorConstructor.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlDisplayNames.h"
#include "IntlDisplayNamesConstructor.h"
#include "IntlDisplayNamesPrototype.h"
#include "IntlDurationFormat.h"
#include "IntlDurationFormatConstructor.h"
#include "IntlDurationFormatPrototype.h"
#include "IntlListFormat.h"
#include "IntlListFormatConstructor.h"
#include "IntlListFormatPrototype.h"
#include "IntlLocale.h"
#include "IntlLocaleConstructor.h"
#include "IntlLocalePrototype.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlObjectInlines.h"
#include "IntlPluralRulesConstructor.h"
#include "IntlPluralRulesPrototype.h"
#include "IntlRelativeTimeFormatConstructor.h"
#include "IntlRelativeTimeFormatPrototype.h"
#include "IntlSegmenter.h"
#include "IntlSegmenterConstructor.h"
#include "IntlSegmenterPrototype.h"
#include "JSCInlines.h"
#include "Options.h"
#include <unicode/ubrk.h>
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/ucurr.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include <wtf/Assertions.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/unicode/icu/ICUHelpers.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlObject);

static JSC_DECLARE_HOST_FUNCTION(intlObjectFuncGetCanonicalLocales);
static JSC_DECLARE_HOST_FUNCTION(intlObjectFuncSupportedValuesOf);

static JSValue createCollatorConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlCollatorConstructor::create(vm, IntlCollatorConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlCollatorPrototype*>(globalObject->collatorStructure()->storedPrototypeObject()));
}

static JSValue createDateTimeFormatConstructor(VM&, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return globalObject->dateTimeFormatConstructor();
}

static JSValue createDisplayNamesConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlDisplayNamesConstructor::create(vm, IntlDisplayNamesConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlDisplayNamesPrototype*>(globalObject->displayNamesStructure()->storedPrototypeObject()));
}

static JSValue createDurationFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlDurationFormatConstructor::create(vm, IntlDurationFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlDurationFormatPrototype*>(globalObject->durationFormatStructure()->storedPrototypeObject()));
}

static JSValue createListFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlListFormatConstructor::create(vm, IntlListFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlListFormatPrototype*>(globalObject->listFormatStructure()->storedPrototypeObject()));
}

static JSValue createLocaleConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlLocaleConstructor::create(vm, IntlLocaleConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlLocalePrototype*>(globalObject->localeStructure()->storedPrototypeObject()));
}

static JSValue createNumberFormatConstructor(VM&, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return globalObject->numberFormatConstructor();
}

static JSValue createPluralRulesConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlPluralRulesConstructor::create(vm, IntlPluralRulesConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlPluralRulesPrototype*>(globalObject->pluralRulesStructure()->storedPrototypeObject()));
}

static JSValue createRelativeTimeFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlRelativeTimeFormatConstructor::create(vm, IntlRelativeTimeFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlRelativeTimeFormatPrototype*>(globalObject->relativeTimeFormatStructure()->storedPrototypeObject()));
}

static JSValue createSegmenterConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject();
    return IntlSegmenterConstructor::create(vm, IntlSegmenterConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlSegmenterPrototype*>(globalObject->segmenterStructure()->storedPrototypeObject()));
}

}

#include "IntlObject.lut.h"

namespace JSC {

/* Source for IntlObject.lut.h
@begin intlObjectTable
  getCanonicalLocales   intlObjectFuncGetCanonicalLocales            DontEnum|Function 1
  supportedValuesOf     intlObjectFuncSupportedValuesOf              DontEnum|Function 1
  Collator              createCollatorConstructor                    DontEnum|PropertyCallback
  DateTimeFormat        createDateTimeFormatConstructor              DontEnum|PropertyCallback
  DisplayNames          createDisplayNamesConstructor                DontEnum|PropertyCallback
  DurationFormat        createDurationFormatConstructor              DontEnum|PropertyCallback
  ListFormat            createListFormatConstructor                  DontEnum|PropertyCallback
  Locale                createLocaleConstructor                      DontEnum|PropertyCallback
  NumberFormat          createNumberFormatConstructor                DontEnum|PropertyCallback
  PluralRules           createPluralRulesConstructor                 DontEnum|PropertyCallback
  RelativeTimeFormat    createRelativeTimeFormatConstructor          DontEnum|PropertyCallback
  Segmenter             createSegmenterConstructor                   DontEnum|PropertyCallback
@end
*/

struct MatcherResult {
    String locale;
    String extension;
    size_t extensionIndex { 0 };
};

const ClassInfo IntlObject::s_info = { "Intl"_s, &Base::s_info, &intlObjectTable, nullptr, CREATE_METHOD_TABLE(IntlObject) };

void UFieldPositionIteratorDeleter::operator()(UFieldPositionIterator* iterator) const
{
    if (iterator)
        ufieldpositer_close(iterator);
}

const MeasureUnit simpleUnits[45] = {
    { "area"_s, "acre"_s },
    { "digital"_s, "bit"_s },
    { "digital"_s, "byte"_s },
    { "temperature"_s, "celsius"_s },
    { "length"_s, "centimeter"_s },
    { "duration"_s, "day"_s },
    { "angle"_s, "degree"_s },
    { "temperature"_s, "fahrenheit"_s },
    { "volume"_s, "fluid-ounce"_s },
    { "length"_s, "foot"_s },
    { "volume"_s, "gallon"_s },
    { "digital"_s, "gigabit"_s },
    { "digital"_s, "gigabyte"_s },
    { "mass"_s, "gram"_s },
    { "area"_s, "hectare"_s },
    { "duration"_s, "hour"_s },
    { "length"_s, "inch"_s },
    { "digital"_s, "kilobit"_s },
    { "digital"_s, "kilobyte"_s },
    { "mass"_s, "kilogram"_s },
    { "length"_s, "kilometer"_s },
    { "volume"_s, "liter"_s },
    { "digital"_s, "megabit"_s },
    { "digital"_s, "megabyte"_s },
    { "length"_s, "meter"_s },
    { "duration"_s, "microsecond"_s },
    { "length"_s, "mile"_s },
    { "length"_s, "mile-scandinavian"_s },
    { "volume"_s, "milliliter"_s },
    { "length"_s, "millimeter"_s },
    { "duration"_s, "millisecond"_s },
    { "duration"_s, "minute"_s },
    { "duration"_s, "month"_s },
    { "duration"_s, "nanosecond"_s },
    { "mass"_s, "ounce"_s },
    { "concentr"_s, "percent"_s },
    { "digital"_s, "petabyte"_s },
    { "mass"_s, "pound"_s },
    { "duration"_s, "second"_s },
    { "mass"_s, "stone"_s },
    { "digital"_s, "terabit"_s },
    { "digital"_s, "terabyte"_s },
    { "duration"_s, "week"_s },
    { "length"_s, "yard"_s },
    { "duration"_s, "year"_s },
};

IntlObject::IntlObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

IntlObject* IntlObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlObject* object = new (NotNull, allocateCell<IntlObject>(vm)) IntlObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

void IntlObject::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

Structure* IntlObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

static Vector<StringView> unicodeExtensionComponents(StringView extension)
{
    // UnicodeExtensionSubtags (extension)
    // https://tc39.github.io/ecma402/#sec-unicodeextensionsubtags

    auto extensionLength = extension.length();
    if (extensionLength < 3)
        return { };

    Vector<StringView> subtags;
    size_t subtagStart = 3; // Skip initial -u-.
    size_t valueStart = 3;
    bool isLeading = true;
    for (size_t index = subtagStart; index < extensionLength; ++index) {
        if (extension[index] == '-') {
            if (index - subtagStart == 2) {
                // Tag is a key, first append prior key's value if there is one.
                if (subtagStart - valueStart > 1)
                    subtags.append(extension.substring(valueStart, subtagStart - valueStart - 1));
                subtags.append(extension.substring(subtagStart, index - subtagStart));
                valueStart = index + 1;
                isLeading = false;
            } else if (isLeading) {
                // Leading subtags before first key.
                subtags.append(extension.substring(subtagStart, index - subtagStart));
                valueStart = index + 1;
            }
            subtagStart = index + 1;
        }
    }
    if (extensionLength - subtagStart == 2) {
        // Trailing an extension key, first append prior key's value if there is one.
        if (subtagStart - valueStart > 1)
            subtags.append(extension.substring(valueStart, subtagStart - valueStart - 1));
        valueStart = subtagStart;
    }
    // Append final key's value.
    subtags.append(extension.substring(valueStart, extensionLength - valueStart));
    return subtags;
}

Vector<char, 32> localeIDBufferForLanguageTagWithNullTerminator(const CString& tag)
{
    if (!tag.length())
        return { };

    UErrorCode status = U_ZERO_ERROR;
    Vector<char, 32> buffer(32);
    int32_t parsedLength;
    auto bufferLength = uloc_forLanguageTag(tag.data(), buffer.mutableSpan().data(), buffer.size(), &parsedLength, &status);
    if (needsToGrowToProduceCString(status)) {
        // Before ICU 64, there's a chance uloc_forLanguageTag will "buffer overflow" while requesting a *smaller* size.
        buffer.resize(bufferLength + 1);
        status = U_ZERO_ERROR;
        uloc_forLanguageTag(tag.data(), buffer.mutableSpan().data(), bufferLength + 1, &parsedLength, &status);
    }
    if (U_FAILURE(status) || parsedLength != static_cast<int32_t>(tag.length()))
        return { };

    ASSERT(buffer.contains('\0'));
    return buffer;
}

Vector<char, 32> canonicalizeUnicodeExtensionsAfterICULocaleCanonicalization(Vector<char, 32>&& buffer)
{
    StringView locale(buffer.span());
    ASSERT(locale.is8Bit());
    size_t extensionIndex = locale.find("-u-"_s);
    if (extensionIndex == notFound)
        return WTFMove(buffer);

    // Since ICU's canonicalization is incomplete, we need to perform some of canonicalization here.
    size_t extensionLength = locale.length() - extensionIndex;
    size_t end = extensionIndex + 3;
    while (end < locale.length()) {
        end = locale.find('-', end);
        if (end == notFound)
            break;
        // Found another singleton.
        if (end + 2 < locale.length() && locale[end + 2] == '-') {
            extensionLength = end - extensionIndex;
            break;
        }
        end++;
    }

    Vector<char, 32> result(buffer.span().first(extensionIndex + 2)); // "-u" is included.
    StringView extension = locale.substring(extensionIndex, extensionLength);
    ASSERT(extension.is8Bit());
    auto subtags = unicodeExtensionComponents(extension);
    for (unsigned index = 0; index < subtags.size();) {
        auto subtag = subtags[index];
        ASSERT(subtag.is8Bit());
        result.append('-');
        result.append(subtag.span8());

        if (subtag.length() != 2) {
            ++index;
            continue;
        }
        ASSERT(subtag.length() == 2);

        // This is unicode extension key.
        unsigned valueIndexStart = index + 1;
        unsigned valueIndexEnd = valueIndexStart;
        for (; valueIndexEnd < subtags.size(); ++valueIndexEnd) {
            if (subtags[valueIndexEnd].length() == 2)
                break;
        }
        // [valueIndexStart, valueIndexEnd) is value of this unicode extension. If there is no value, valueIndexStart == valueIndexEnd.

        for (unsigned valueIndex = valueIndexStart; valueIndex < valueIndexEnd; ++valueIndex) {
            auto value = subtags[valueIndex];
            if (value != "true"_s) {
                result.append('-');
                result.append(value.span8());
            }
        }
        index = valueIndexEnd;
    }

    result.append(buffer.subspan(extensionIndex + extensionLength));
    return result;
}

String languageTagForLocaleID(const char* localeID, bool isImmortal)
{
    Vector<char, 32> buffer;
    auto status = callBufferProducingFunction(uloc_toLanguageTag, localeID, buffer, false);
    if (U_FAILURE(status))
        return String();

    auto createResult = [&](Vector<char, 32>&& buffer) -> String {
        // This is used to store into static variables that may be shared across JSC execution threads.
        // This must be immortal to make concurrent ref/deref safe.
        if (isImmortal)
            return StringImpl::createStaticStringImpl(buffer.span());
        return buffer.span();
    };

    return createResult(canonicalizeUnicodeExtensionsAfterICULocaleCanonicalization(WTFMove(buffer)));
}

// Ensure we have xx-ZZ whenever we have xx-Yyyy-ZZ.
static void addScriptlessLocaleIfNeeded(LocaleSet& availableLocales, StringView locale)
{
    if (locale.length() < 10)
        return;

    Vector<StringView, 3> subtags;
    for (auto subtag : locale.split('-')) {
        if (subtags.size() == 3)
            return;
        subtags.append(subtag);
    }

    if (subtags.size() != 3 || subtags[1].length() != 4 || subtags[2].length() > 3)
        return;

    Vector<char, 12> buffer;
    ASSERT(subtags[0].is8Bit() && subtags[0].containsOnlyASCII());
    buffer.append(subtags[0].span8());
    buffer.append('-');
    ASSERT(subtags[2].is8Bit() && subtags[2].containsOnlyASCII());
    buffer.append(subtags[2].span8());

    availableLocales.add(StringImpl::createStaticStringImpl(buffer.span()));
}

const LocaleSet& intlAvailableLocales()
{
    static LazyNeverDestroyed<LocaleSet> availableLocales;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableLocales.construct();
        ASSERT(availableLocales->isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = uloc_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(uloc_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales->add(locale);
            addScriptlessLocaleIfNeeded(availableLocales.get(), locale);
        }
    });
    return availableLocales;
}

// This table is total ordering indexes for ASCII characters in UCA DUCET.
// It is generated from CLDR common/uca/allkeys_DUCET.txt.
//
// Rough overview of UCA is the followings.
// https://unicode.org/reports/tr10/#Main_Algorithm
//
//     1. Normalize each input string.
//
//     2. Produce an array of collation elements for each string.
//
//         There are 3 (or 4) levels. And each character has 4 weights. We concatenate them into one sequence called collation elements.
//         For example, "c" has `[.0706.0020.0002]`. And "ca◌́b" becomes `[.0706.0020.0002], [.06D9.0020.0002], [.0000.0021.0002], [.06EE.0020.0002]`
//         We need to consider variable weighting (https://unicode.org/reports/tr10/#Variable_Weighting), but if it is Non-ignorable, we can just use
//         the collation elements defined in the table.
//
//     3. Produce a sort key for each string from the arrays of collation elements.
//
//         Generate sort key from collation elements. From lower levels to higher levels, we collect weights. But 0000 weight is skipped.
//         Between levels, we insert 0000 weight if the boundary.
//
//             string: "ca◌́b"
//             collation elements: `[.0706.0020.0002], [.06D9.0020.0002], [.0000.0021.0002], [.06EE.0020.0002]`
//             sort key: `0706 06D9 06EE 0000 0020 0020 0021 0020 0000 0002 0002 0002 0002`
//                                        ^                        ^
//                                        level boundary           level boundary
//
//     4. Compare the two sort keys with a binary comparison operation.
//
// Key observations are the followings.
//
//     1. If an input is an ASCII string, UCA step-1 normalization does nothing.
//     2. If an input is an ASCII string, non-starters (https://unicode.org/reports/tr10/#UTS10-D33) does not exist. So no special handling in UCA step-2 is required.
//     3. If an input is an ASCII string, no multiple character collation elements exist. So no special handling in UCA step-2 is required. For example, "L·" is not ASCII.
//     4. UCA step-3 handles 0000 weighted characters specially. And ASCII contains these characters. But 0000 elements are used only for rare control characters.
//        We can ignore this special handling if ASCII strings do not include control characters.
//     5. Level-1 weights are different except for 0000 cases and capital / lower ASCII characters. All non-0000 elements are larger than 0000.
//     6. Level-2 weights are always 0020 except for 0000 cases. So if we include 0000 characters, we do not need to perform level-2 weight comparison.
//     7. In all levels, characters have non-0000 weights if it does not have 0000 weight in level-1.
//     8. In level-1, weights are the same only when characters are the same latin letters ('A' v.s. 'a'). If level-1 weight comparison says EQUAL, and if characters are not binary-equal,
//        then, the only case is they are including the same latin letters with different capitalization at the same position. Level-3 weight comparison must distinguish them since level-3
//        weight is set only for latin capital letters. Thus, we do not need to perform level-4 weight comparison.
//
//  Based on the above observation, our fast path handles ASCII strings excluding control characters. We first compare strings with level-1 weights. And then,
//  if we found they are the same and if we found they are not binary-equal strings, then we perform comparison with level-3 and level-4 weights.
const uint8_t ducetLevel1Weights[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    6, 12, 16, 28, 38, 29, 27, 15,
    17, 18, 24, 32, 9, 8, 14, 25,
    39, 40, 41, 42, 43, 44, 45, 46,
    47, 48, 11, 10, 33, 34, 35, 13,
    23, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71,
    72, 73, 74, 19, 26, 20, 31, 7,
    30, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71,
    72, 73, 74, 21, 36, 22, 37, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

// Level 2 are all zeros.

const uint8_t ducetLevel3Weights[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const LocaleSet& intlCollatorAvailableLocales()
{
    static LazyNeverDestroyed<LocaleSet> availableLocales;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableLocales.construct();
        ASSERT(availableLocales->isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = ucol_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(ucol_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales->add(locale);
            addScriptlessLocaleIfNeeded(availableLocales.get(), locale);
        }
        IntlCollator::checkICULocaleInvariants(availableLocales.get());
    });
    return availableLocales;
}

const LocaleSet& intlSegmenterAvailableLocales()
{
    static LazyNeverDestroyed<LocaleSet> availableLocales;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableLocales.construct();
        ASSERT(availableLocales->isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = ubrk_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(ubrk_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales->add(locale);
            addScriptlessLocaleIfNeeded(availableLocales.get(), locale);
        }
    });
    return availableLocales;
}

// https://tc39.es/ecma402/#sec-getoption
TriState intlBooleanOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return TriState::Indeterminate;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, TriState::Indeterminate);

    if (value.isUndefined())
        return TriState::Indeterminate;

    return triState(value.toBoolean(globalObject));
}

String intlStringOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property, std::initializer_list<ASCIILiteral> values, ASCIILiteral notFound, ASCIILiteral fallback)
{
    // GetOption (options, property, type="string", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return fallback;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, String());

    if (!value.isUndefined()) {
        String stringValue = value.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, String());

        if (values.size() && std::find(values.begin(), values.end(), stringValue) == values.end()) {
            throwException(globalObject, scope, createRangeError(globalObject, notFound));
            return { };
        }
        return stringValue;
    }

    return fallback;
}

unsigned intlNumberOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // GetNumberOption (options, property, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-getnumberoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return fallback;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, 0);

    RELEASE_AND_RETURN(scope, intlDefaultNumberOption(globalObject, value, property, minimum, maximum, fallback));
}

unsigned intlDefaultNumberOption(JSGlobalObject* globalObject, JSValue value, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // DefaultNumberOption (value, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-defaultnumberoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isUndefined()) {
        double doubleValue = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);

        if (!(doubleValue >= minimum && doubleValue <= maximum)) {
            throwException(globalObject, scope, createRangeError(globalObject, makeString(property.publicName(), " is out of range"_s)));
            return 0;
        }
        return static_cast<unsigned>(doubleValue);
    }
    return fallback;
}

// http://www.unicode.org/reports/tr35/#Unicode_locale_identifier
bool isUnicodeLocaleIdentifierType(StringView string)
{
    return readCharactersForParsing(string, [](auto buffer) -> bool {
        while (true) {
            auto begin = buffer.position();
            while (buffer.hasCharactersRemaining() && isASCIIAlphanumeric(*buffer))
                ++buffer;
            unsigned length = buffer.position() - begin;
            if (length < 3 || length > 8)
                return false;
            if (!buffer.hasCharactersRemaining())
                return true;
            if (*buffer != '-')
                return false;
            ++buffer;
        }
    });
}

// https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid
String canonicalizeUnicodeLocaleID(const CString& tag)
{
    auto buffer = localeIDBufferForLanguageTagWithNullTerminator(tag);
    if (buffer.isEmpty())
        return String();
    auto canonicalized = canonicalizeLocaleIDWithoutNullTerminator(buffer.span().data());
    if (!canonicalized)
        return String();
    canonicalized->append('\0');
    ASSERT(canonicalized->contains('\0'));
    return languageTagForLocaleID(canonicalized->span().data());
}

Vector<String> canonicalizeLocaleList(JSGlobalObject* globalObject, JSValue locales)
{
    // CanonicalizeLocaleList (locales)
    // https://tc39.github.io/ecma402/#sec-canonicalizelocalelist

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> seen;

    if (locales.isUndefined())
        return seen;

    JSObject* localesObject;
    if (locales.isString() || locales.inherits<IntlLocale>()) {
        JSArray* localesArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous));
        if (!localesArray) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        localesArray->push(globalObject, locales);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        localesObject = localesArray;
    } else {
        localesObject = locales.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, Vector<String>());
    }

    // 6. Let len be ToLength(Get(O, "length")).
    JSValue lengthProperty = localesObject->get(globalObject, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    uint64_t length = lengthProperty.toLength(globalObject);
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    UncheckedKeyHashSet<String> seenSet;
    for (uint64_t k = 0; k < length; ++k) {
        bool kPresent = localesObject->hasProperty(globalObject, k);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        if (kPresent) {
            JSValue kValue = localesObject->get(globalObject, k);
            RETURN_IF_EXCEPTION(scope, Vector<String>());

            if (!kValue.isString() && !kValue.isObject()) {
                throwTypeError(globalObject, scope, "locale value must be a string or object"_s);
                return { };
            }

            String tag;
            if (kValue.inherits<IntlLocale>())
                tag = jsCast<IntlLocale*>(kValue)->toString();
            else {
                JSString* string = kValue.toString(globalObject);
                RETURN_IF_EXCEPTION(scope, Vector<String>());

                tag = string->value(globalObject);
                RETURN_IF_EXCEPTION(scope, Vector<String>());
            }

            if (isStructurallyValidLanguageTag(tag)) {
                ASSERT(tag.containsOnlyASCII());
                String canonicalizedTag = canonicalizeUnicodeLocaleID(tag.ascii());
                if (!canonicalizedTag.isNull()) {
                    if (seenSet.add(canonicalizedTag).isNewEntry)
                        seen.append(canonicalizedTag);
                    continue;
                }
            }

            String errorMessage = tryMakeString("invalid language tag: "_s, tag);
            if (!errorMessage) [[unlikely]] {
                throwException(globalObject, scope, createOutOfMemoryError(globalObject));
                return { };
            }
            throwException(globalObject, scope, createRangeError(globalObject, errorMessage));
            return { };
        }
    }

    return seen;
}

String bestAvailableLocale(const LocaleSet& availableLocales, const String& locale)
{
    return bestAvailableLocale(locale, [&](const String& candidate) {
        return availableLocales.contains(candidate);
    });
}

String defaultLocale(JSGlobalObject* globalObject)
{
    // DefaultLocale ()
    // https://tc39.github.io/ecma402/#sec-defaultlocale

    // WebCore's global objects will have their own ideas of how to determine the language. It may
    // be determined by WebCore-specific logic like some WK settings. Usually this will return the
    // same thing as userPreferredLanguages()[0].
    if (auto defaultLanguage = globalObject->globalObjectMethodTable()->defaultLanguage) {
        String locale = canonicalizeUnicodeLocaleID(defaultLanguage().utf8());
        if (!locale.isEmpty())
            return locale;
    }

    Vector<String> languages = userPreferredLanguages();
    for (const auto& language : languages) {
        String locale = canonicalizeUnicodeLocaleID(language.utf8());
        if (!locale.isEmpty())
            return locale;
    }

    // If all else fails, ask ICU. It will probably say something bogus like en_us even if the user
    // has configured some other language, but being wrong is better than crashing.
    static LazyNeverDestroyed<String> icuDefaultLocalString;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        constexpr bool isImmortal = true;
        icuDefaultLocalString.construct(languageTagForLocaleID(uloc_getDefault(), isImmortal));
    });
    if (!icuDefaultLocalString->isEmpty())
        return icuDefaultLocalString.get();

    return "en"_s;
}

String removeUnicodeLocaleExtension(const String& locale)
{
    Vector<String> parts = locale.split('-');
    StringBuilder builder;
    size_t partsSize = parts.size();
    bool atPrivate = false;
    if (partsSize > 0)
        builder.append(parts[0]);
    for (size_t p = 1; p < partsSize; ++p) {
        if (parts[p] == "x"_s)
            atPrivate = true;
        if (!atPrivate && parts[p] == "u"_s && p + 1 < partsSize) {
            // Skip the u- and anything that follows until another singleton.
            // While the next part is part of the unicode extension, skip it.
            while (p + 1 < partsSize && parts[p + 1].length() > 1)
                ++p;
        } else {
            builder.append('-', parts[p]);
        }
    }
    return builder.toString();
}

static MatcherResult lookupMatcher(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales)
{
    // LookupMatcher (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-lookupmatcher

    String locale;
    String noExtensionsLocale;
    String availableLocale;
    for (size_t i = 0; i < requestedLocales.size() && availableLocale.isNull(); ++i) {
        locale = requestedLocales[i];
        noExtensionsLocale = removeUnicodeLocaleExtension(locale);
        availableLocale = bestAvailableLocale(availableLocales, noExtensionsLocale);
    }

    MatcherResult result;
    if (!availableLocale.isEmpty()) {
        result.locale = availableLocale;
        if (locale != noExtensionsLocale) {
            size_t extensionIndex = locale.find("-u-"_s);
            RELEASE_ASSERT(extensionIndex != notFound);

            size_t extensionLength = locale.length() - extensionIndex;
            size_t end = extensionIndex + 3;
            while (end < locale.length()) {
                end = locale.find('-', end);
                if (end == notFound)
                    break;
                if (end + 2 < locale.length() && locale[end + 2] == '-') {
                    extensionLength = end - extensionIndex;
                    break;
                }
                end++;
            }
            result.extension = locale.substring(extensionIndex, extensionLength);
            result.extensionIndex = extensionIndex;
        }
    } else
        result.locale = defaultLocale(globalObject);
    return result;
}

static MatcherResult bestFitMatcher(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitMatcher (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitmatcher

    // FIXME: Implement something better than lookup.
    return lookupMatcher(globalObject, availableLocales, requestedLocales);
}

constexpr ASCIILiteral relevantExtensionKeyString(RelevantExtensionKey key)
{
    switch (key) {
#define JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS(lowerName, capitalizedName) \
    case RelevantExtensionKey::capitalizedName: \
        return #lowerName ""_s;
    JSC_INTL_RELEVANT_EXTENSION_KEYS(JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS)
#undef JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS
    }
    return { };
}

ResolvedLocale resolveLocale(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales, LocaleMatcher localeMatcher, const ResolveLocaleOptions& options, std::initializer_list<RelevantExtensionKey> relevantExtensionKeys, Vector<String> (*localeData)(const String&, RelevantExtensionKey))
{
    // ResolveLocale (availableLocales, requestedLocales, options, relevantExtensionKeys, localeData)
    // https://tc39.github.io/ecma402/#sec-resolvelocale

    MatcherResult matcherResult = localeMatcher == LocaleMatcher::Lookup
        ? lookupMatcher(globalObject, availableLocales, requestedLocales)
        : bestFitMatcher(globalObject, availableLocales, requestedLocales);

    String foundLocale = matcherResult.locale;

    Vector<StringView> extensionSubtags;
    if (!matcherResult.extension.isNull())
        extensionSubtags = unicodeExtensionComponents(matcherResult.extension);

    ResolvedLocale resolved;
    resolved.dataLocale = foundLocale;

    StringBuilder supportedExtension;
    supportedExtension.append("-u"_s);
    for (RelevantExtensionKey key : relevantExtensionKeys) {
        ASCIILiteral keyString = relevantExtensionKeyString(key);
        Vector<String> keyLocaleData = localeData(foundLocale, key);
        ASSERT(!keyLocaleData.isEmpty());

        String value = keyLocaleData[0];
        String supportedExtensionAddition;

        if (!extensionSubtags.isEmpty()) {
            size_t keyPos = extensionSubtags.find(keyString);
            if (keyPos != notFound) {
                if (keyPos + 1 < extensionSubtags.size() && extensionSubtags[keyPos + 1].length() > 2) {
                    StringView requestedValue = extensionSubtags[keyPos + 1];
                    auto dataPos = keyLocaleData.find(requestedValue);
                    if (dataPos != notFound) {
                        value = keyLocaleData[dataPos];
                        supportedExtensionAddition = makeString('-', keyString, '-', value);
                    }
                } else if (keyLocaleData.contains("true"_s)) {
                    value = "true"_s;
                    supportedExtensionAddition = makeString('-', keyString);
                }
            }
        }

        if (auto optionsValue = options[static_cast<unsigned>(key)]) {
            // Undefined should not get added to the options, it won't displace the extension.
            // Null will remove the extension.
            if ((optionsValue->isNull() || keyLocaleData.contains(*optionsValue)) && *optionsValue != value) {
                value = optionsValue.value();
                supportedExtensionAddition = String();
            }
        }
        resolved.extensions[static_cast<unsigned>(key)] = value;
        supportedExtension.append(supportedExtensionAddition);
    }

    if (supportedExtension.length() > 2) {
        StringView foundLocaleView(foundLocale);
        foundLocale = makeString(foundLocaleView.left(matcherResult.extensionIndex), supportedExtension.toString(), foundLocaleView.substring(matcherResult.extensionIndex));
    }

    resolved.locale = WTFMove(foundLocale);
    return resolved;
}

static JSArray* lookupSupportedLocales(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales)
{
    // LookupSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-lookupsupportedlocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t len = requestedLocales.size();
    JSArray* subset = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!subset) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    unsigned index = 0;
    for (size_t k = 0; k < len; ++k) {
        const String& locale = requestedLocales[k];
        String noExtensionsLocale = removeUnicodeLocaleExtension(locale);
        String availableLocale = bestAvailableLocale(availableLocales, noExtensionsLocale);
        if (!availableLocale.isNull()) {
            subset->putDirectIndex(globalObject, index++, jsString(vm, locale));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    return subset;
}

static JSArray* bestFitSupportedLocales(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitsupportedlocales

    // FIXME: Implement something better than lookup.
    return lookupSupportedLocales(globalObject, availableLocales, requestedLocales);
}

JSValue supportedLocales(JSGlobalObject* globalObject, const LocaleSet& availableLocales, const Vector<String>& requestedLocales, JSValue optionsValue)
{
    // SupportedLocales (availableLocales, requestedLocales, options)
    // https://tc39.github.io/ecma402/#sec-supportedlocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String matcher;

    JSObject* options = intlCoerceOptionsToObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, JSValue());

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (localeMatcher == LocaleMatcher::BestFit)
        RELEASE_AND_RETURN(scope, bestFitSupportedLocales(globalObject, availableLocales, requestedLocales));
    RELEASE_AND_RETURN(scope, lookupSupportedLocales(globalObject, availableLocales, requestedLocales));
}

Vector<String> numberingSystemsForLocale(const String& locale)
{
    static LazyNeverDestroyed<Vector<String>> availableNumberingSystems;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableNumberingSystems.construct();
        ASSERT(availableNumberingSystems->isEmpty());
        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* numberingSystemNames = unumsys_openAvailableNames(&status);
        ASSERT(U_SUCCESS(status));

        int32_t resultLength;
        // Numbering system names are always ASCII, so use char[].
        while (const char* result = uenum_next(numberingSystemNames, &resultLength, &status)) {
            ASSERT(U_SUCCESS(status));
            auto numsys = unumsys_openByName(result, &status);
            ASSERT(U_SUCCESS(status));
            // Only support algorithmic if it is the default fot the locale, handled below.
            if (!unumsys_isAlgorithmic(numsys))
                availableNumberingSystems->append(String(StringImpl::createStaticStringImpl({ result, static_cast<size_t>(resultLength) })));
            unumsys_close(numsys);
        }
        uenum_close(numberingSystemNames);
    });

    UErrorCode status = U_ZERO_ERROR;
    UNumberingSystem* defaultSystem = unumsys_open(locale.utf8().data(), &status);
    ASSERT(U_SUCCESS(status));
    auto defaultSystemName = String::fromLatin1(unumsys_getName(defaultSystem));
    unumsys_close(defaultSystem);

    Vector<String> numberingSystems({ defaultSystemName });
    numberingSystems.appendVector(availableNumberingSystems.get());
    return numberingSystems;
}

// unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
bool isUnicodeLanguageSubtag(StringView string)
{
    auto length = string.length();
    return length >= 2 && length <= 8 && length != 4 && string.containsOnly<isASCIIAlpha>();
}

// unicode_script_subtag = alpha{4} ;
bool isUnicodeScriptSubtag(StringView string)
{
    return string.length() == 4 && string.containsOnly<isASCIIAlpha>();
}

// unicode_region_subtag = alpha{2} | digit{3} ;
bool isUnicodeRegionSubtag(StringView string)
{
    auto length = string.length();
    return (length == 2 && string.containsOnly<isASCIIAlpha>())
        || (length == 3 && string.containsOnly<isASCIIDigit>());
}

// unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
bool isUnicodeVariantSubtag(StringView string)
{
    auto length = string.length();
    if (length >= 5 && length <= 8)
        return string.containsOnly<isASCIIAlphanumeric>();
    return length == 4 && isASCIIDigit(string[0]) && string.substring(1).containsOnly<isASCIIAlphanumeric>();
}

using VariantCode = uint64_t;
static VariantCode parseVariantCode(StringView string)
{
    ASSERT(isUnicodeVariantSubtag(string));
    ASSERT(string.containsOnlyASCII());
    ASSERT(string.length() <= 8);
    ASSERT(string.length() >= 1);
    struct Code {
        LChar characters[8] { };
    };
    static_assert(std::is_unsigned_v<LChar>);
    static_assert(sizeof(VariantCode) == sizeof(Code));
    Code code { };
    for (unsigned index = 0; index < string.length(); ++index)
        code.characters[index] = toASCIILower(string[index]);
    VariantCode result = std::bit_cast<VariantCode>(code);
    ASSERT(result); // Not possible since some characters exist.
    ASSERT(result != static_cast<VariantCode>(-1)); // Not possible since all characters are ASCII (not Latin-1).
    return result;
}

static unsigned convertToUnicodeSingletonIndex(char16_t singleton)
{
    ASSERT(isASCIIAlphanumeric(singleton));
    singleton = toASCIILower(singleton);
    // 0 - 9 => numeric
    // 10 - 35 => alpha
    if (isASCIIDigit(singleton))
        return singleton - '0';
    return (singleton - 'a') + 10;
}
static constexpr unsigned numberOfUnicodeSingletons = 10 + 26; // Digits + Alphabets.

static bool isUnicodeExtensionAttribute(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.containsOnly<isASCIIAlphanumeric>();
}

static bool isUnicodeExtensionKey(StringView string)
{
    return string.length() == 2 && isASCIIAlphanumeric(string[0]) && isASCIIAlpha(string[1]);
}

static bool isUnicodeExtensionTypeComponent(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.containsOnly<isASCIIAlphanumeric>();
}

static bool isUnicodePUExtensionValue(StringView string)
{
    auto length = string.length();
    return length >= 1 && length <= 8 && string.containsOnly<isASCIIAlphanumeric>();
}

static bool isUnicodeOtherExtensionValue(StringView string)
{
    auto length = string.length();
    return length >= 2 && length <= 8 && string.containsOnly<isASCIIAlphanumeric>();
}

static bool isUnicodeTKey(StringView string)
{
    return string.length() == 2 && isASCIIAlpha(string[0]) && isASCIIDigit(string[1]);
}

static bool isUnicodeTValueComponent(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.containsOnly<isASCIIAlphanumeric>();
}

// The IsStructurallyValidLanguageTag abstract operation verifies that the locale argument (which must be a String value)
//
//     represents a well-formed "Unicode BCP 47 locale identifier" as specified in Unicode Technical Standard 35 section 3.2,
//     does not include duplicate variant subtags, and
//     does not include duplicate singleton subtags.
//
//  The abstract operation returns true if locale can be generated from the EBNF grammar in section 3.2 of the Unicode Technical Standard 35,
//  starting with unicode_locale_id, and does not contain duplicate variant or singleton subtags (other than as a private use subtag).
//  It returns false otherwise. Terminal value characters in the grammar are interpreted as the Unicode equivalents of the ASCII octet values given.
//
// https://unicode.org/reports/tr35/#Unicode_locale_identifier
class LanguageTagParser {
public:
    LanguageTagParser(StringView tag)
        : m_range(tag.splitAllowingEmptyEntries('-'))
        , m_cursor(m_range.begin())
    {
        ASSERT(m_cursor != m_range.end());
        m_current = *m_cursor;
    }

    bool parseUnicodeLocaleId();
    bool parseUnicodeLanguageId();

    bool isEOS()
    {
        return m_cursor == m_range.end();
    }

    bool next()
    {
        if (isEOS())
            return false;

        ++m_cursor;
        if (isEOS()) {
            m_current = StringView();
            return false;
        }
        m_current = *m_cursor;
        return true;
    }

private:
    bool parseExtensionsAndPUExtensions();

    bool parseUnicodeExtensionAfterPrefix();
    bool parseTransformedExtensionAfterPrefix();
    bool parseOtherExtensionAfterPrefix();
    bool parsePUExtensionAfterPrefix();

    StringView::SplitResult m_range;
    StringView::SplitResult::Iterator m_cursor;
    StringView m_current;
};

bool LanguageTagParser::parseUnicodeLocaleId()
{
    // unicode_locale_id    = unicode_language_id
    //                        extensions*
    //                        pu_extensions? ;
    ASSERT(!isEOS());
    if (!parseUnicodeLanguageId())
        return false;
    if (isEOS())
        return true;
    if (!parseExtensionsAndPUExtensions())
        return false;
    return true;
}

bool LanguageTagParser::parseUnicodeLanguageId()
{
    // unicode_language_id  = unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
    ASSERT(!isEOS());
    if (!isUnicodeLanguageSubtag(m_current))
        return false;
    if (!next())
        return true;

    if (isUnicodeScriptSubtag(m_current)) {
        if (!next())
            return true;
    }

    if (isUnicodeRegionSubtag(m_current)) {
        if (!next())
            return true;
    }

    UncheckedKeyHashSet<VariantCode> variantCodes;
    while (true) {
        if (!isUnicodeVariantSubtag(m_current))
            return true;
        // https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
        // does not include duplicate variant subtags
        if (!variantCodes.add(parseVariantCode(m_current)).isNewEntry)
            return false;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parseUnicodeExtensionAfterPrefix()
{
    // ((sep keyword)+ | (sep attribute)+ (sep keyword)*) ;
    //
    // keyword = key (sep type)? ;
    // key = alphanum alpha ;
    // type = alphanum{3,8} (sep alphanum{3,8})* ;
    // attribute = alphanum{3,8} ;
    ASSERT(!isEOS());
    bool isAttributeOrKeyword = false;
    if (isUnicodeExtensionAttribute(m_current)) {
        isAttributeOrKeyword = true;
        while (true) {
            if (!isUnicodeExtensionAttribute(m_current))
                break;
            if (!next())
                return true;
        }
    }

    if (isUnicodeExtensionKey(m_current)) {
        isAttributeOrKeyword = true;
        while (true) {
            if (!isUnicodeExtensionKey(m_current))
                break;
            if (!next())
                return true;
            while (true) {
                if (!isUnicodeExtensionTypeComponent(m_current))
                    break;
                if (!next())
                    return true;
            }
        }
    }

    if (!isAttributeOrKeyword)
        return false;
    return true;
}

bool LanguageTagParser::parseTransformedExtensionAfterPrefix()
{
    // ((sep tlang (sep tfield)*) | (sep tfield)+) ;
    //
    // tlang = unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
    // tfield = tkey tvalue;
    // tkey = alpha digit ;
    // tvalue = (sep alphanum{3,8})+ ;
    ASSERT(!isEOS());
    bool found = false;
    if (isUnicodeLanguageSubtag(m_current)) {
        found = true;
        if (!parseUnicodeLanguageId())
            return false;
        if (isEOS())
            return true;
    }

    if (isUnicodeTKey(m_current)) {
        found = true;
        while (true) {
            if (!isUnicodeTKey(m_current))
                break;
            if (!next())
                return false;
            if (!isUnicodeTValueComponent(m_current))
                return false;
            if (!next())
                return true;
            while (true) {
                if (!isUnicodeTValueComponent(m_current))
                    break;
                if (!next())
                    return true;
            }
        }
    }

    return found;
}

bool LanguageTagParser::parseOtherExtensionAfterPrefix()
{
    // (sep alphanum{2,8})+ ;
    ASSERT(!isEOS());
    if (!isUnicodeOtherExtensionValue(m_current))
        return false;
    if (!next())
        return true;

    while (true) {
        if (!isUnicodeOtherExtensionValue(m_current))
            return true;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parsePUExtensionAfterPrefix()
{
    // (sep alphanum{1,8})+ ;
    ASSERT(!isEOS());
    if (!isUnicodePUExtensionValue(m_current))
        return false;
    if (!next())
        return true;

    while (true) {
        if (!isUnicodePUExtensionValue(m_current))
            return true;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parseExtensionsAndPUExtensions()
{
    // unicode_locale_id    = unicode_language_id
    //                        extensions*
    //                        pu_extensions? ;
    //
    // extensions = unicode_locale_extensions
    //            | transformed_extensions
    //            | other_extensions ;
    //
    // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
    ASSERT(!isEOS());
    WTF::BitSet<numberOfUnicodeSingletons> singletonsSet { };
    while (true) {
        if (m_current.length() != 1)
            return true;
        char16_t prefixCode = m_current[0];
        if (!isASCIIAlphanumeric(prefixCode))
            return true;

        // https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
        // does not include duplicate singleton subtags.
        //
        // https://unicode.org/reports/tr35/#Unicode_locale_identifier
        // As is often the case, the complete syntactic constraints are not easily captured by ABNF,
        // so there is a further condition: There cannot be more than one extension with the same singleton (-a-, …, -t-, -u-, …).
        // Note that the private use extension (-x-) must come after all other extensions.
        if (singletonsSet.get(convertToUnicodeSingletonIndex(prefixCode)))
            return false;
        singletonsSet.set(convertToUnicodeSingletonIndex(prefixCode), true);

        switch (prefixCode) {
        case 'u':
        case 'U': {
            // unicode_locale_extensions = sep [uU] ((sep keyword)+ | (sep attribute)+ (sep keyword)*) ;
            if (!next())
                return false;
            if (!parseUnicodeExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        case 't':
        case 'T': {
            // transformed_extensions = sep [tT] ((sep tlang (sep tfield)*) | (sep tfield)+) ;
            if (!next())
                return false;
            if (!parseTransformedExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        case 'x':
        case 'X': {
            // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
            if (!next())
                return false;
            if (!parsePUExtensionAfterPrefix())
                return false;
            return true; // If pu_extensions appear, no extensions can follow after that. This must be the end of unicode_locale_id.
        }
        default: {
            // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
            if (!next())
                return false;
            if (!parseOtherExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        }
    }
}

// https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
bool isStructurallyValidLanguageTag(StringView string)
{
    LanguageTagParser parser(string);
    if (!parser.parseUnicodeLocaleId())
        return false;
    if (!parser.isEOS())
        return false;
    return true;
}

// unicode_language_id, but intersection of BCP47 and UTS35.
// unicode_language_id =
//     | unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
// https://github.com/tc39/proposal-intl-displaynames/issues/79
bool isUnicodeLanguageId(StringView string)
{
    LanguageTagParser parser(string);
    if (!parser.parseUnicodeLanguageId())
        return false;
    if (!parser.isEOS())
        return false;
    return true;
}

bool isWellFormedCurrencyCode(StringView currency)
{
    return currency.length() == 3 && currency.containsOnly<isASCIIAlpha>();
}

std::optional<Vector<char, 32>> canonicalizeLocaleIDWithoutNullTerminator(const char* localeID)
{
    ASSERT(localeID);
    Vector<char, 32> buffer;
#if U_ICU_VERSION_MAJOR_NUM >= 68 && USE(APPLE_INTERNAL_SDK)
    // Use ualoc_canonicalForm AppleICU SPI, which can perform mapping of aliases.
    // ICU-21506 is a bug upstreaming this SPI to ICU.
    // https://unicode-org.atlassian.net/browse/ICU-21506
    auto status = callBufferProducingFunction(ualoc_canonicalForm, localeID, buffer);
    if (U_FAILURE(status))
        return std::nullopt;
    return buffer;
#else
    auto status = callBufferProducingFunction(uloc_canonicalize, localeID, buffer);
    if (U_FAILURE(status))
        return std::nullopt;
    return buffer;
#endif
}

std::optional<String> mapICUCalendarKeywordToBCP47(const String& calendar)
{
    if (calendar == "gregorian"_s)
        return "gregory"_s;
    if (calendar == "ethiopic-amete-alem"_s)
        return "ethioaa"_s;
    return std::nullopt;
}

std::optional<String> mapBCP47ToICUCalendarKeyword(const String& calendar)
{
    if (calendar == "gregory"_s)
        return "gregorian"_s;
    if (calendar == "ethioaa"_s)
        return "ethiopic-amete-alem"_s;
    return std::nullopt;
}

std::optional<String> mapICUCollationKeywordToBCP47(const String& collation)
{
    if (collation == "dictionary"_s)
        return "dict"_s;
    if (collation == "gb2312han"_s)
        return "gb2312"_s;
    if (collation == "phonebook"_s)
        return "phonebk"_s;
    if (collation == "traditional"_s)
        return "trad"_s;
    return std::nullopt;
}

JSC_DEFINE_HOST_FUNCTION(intlObjectFuncGetCanonicalLocales, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // Intl.getCanonicalLocales(locales)
    // https://tc39.github.io/ecma402/#sec-intl.getcanonicallocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> localeList = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto length = localeList.size();

    JSArray* localeArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), length);
    if (!localeArray) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    for (size_t i = 0; i < length; ++i) {
        localeArray->putDirectIndex(globalObject, i, jsString(vm, localeList[i]));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(localeArray);
}

const Vector<String>& intlAvailableCalendars()
{
    static LazyNeverDestroyed<Vector<String>> availableCalendars;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        UErrorCode status = U_ZERO_ERROR;
        auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucal_getKeywordValuesForLocale("calendars", "und", false, &status));
        ASSERT(U_SUCCESS(status));

        int32_t count = uenum_count(enumeration.get(), &status);
        ASSERT(U_SUCCESS(status));

        auto createImmortalThreadSafeString = [&](String&& string) {
            if (string.is8Bit())
                return StringImpl::createStaticStringImpl(string.span8());
            return StringImpl::createStaticStringImpl(string.span16());
        };

        availableCalendars.construct(count, [&](size_t) {
            int32_t length = 0;
            const char* pointer = uenum_next(enumeration.get(), &length, &status);
            ASSERT(U_SUCCESS(status));
            String calendar(unsafeMakeSpan(pointer, static_cast<size_t>(length)));
            if (auto mapped = mapICUCalendarKeywordToBCP47(calendar))
                return createImmortalThreadSafeString(WTFMove(mapped.value()));
            return createImmortalThreadSafeString(WTFMove(calendar));
        });

        // The AvailableCalendars abstract operation returns a List, ordered as if an Array of the same
        // values had been sorted using %Array.prototype.sort% using undefined as comparator
        std::sort(availableCalendars->begin(), availableCalendars->end(),
            [](const String& a, const String& b) {
                return WTF::codePointCompare(a, b) < 0;
            });
    });
    return availableCalendars;
}

CalendarID iso8601CalendarIDStorage { std::numeric_limits<CalendarID>::max() };
CalendarID iso8601CalendarIDSlow()
{
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        const auto& calendars = intlAvailableCalendars();
        for (unsigned index = 0; index < calendars.size(); ++index) {
            if (calendars[index] == "iso8601"_s) {
                iso8601CalendarIDStorage = index;
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    });
    return iso8601CalendarIDStorage;
}

// https://tc39.es/proposal-intl-enumeration/#sec-availablecalendars
static JSArray* availableCalendars(JSGlobalObject* globalObject)
{
    return createArrayFromStringVector(globalObject, intlAvailableCalendars());
}

// https://tc39.es/proposal-intl-enumeration/#sec-availablecollations
static JSArray* availableCollations(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucol_getKeywordValues("collation", &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available collations"_s);
        return { };
    }

    int32_t count = uenum_count(enumeration.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available collations"_s);
        return { };
    }

    Vector<String, 1> elements;
    elements.reserveInitialCapacity(count + 2);
    // ICU ~69 has a bug that does not report "emoji" and "eor" for collation when using ucol_getKeywordValues.
    // https://github.com/unicode-org/icu/commit/24778dfc9bf67f431509361a173a33a1ab860b5d
    elements.append("emoji"_s);
    elements.append("eor"_s);
    for (int32_t index = 0; index < count; ++index) {
        int32_t length = 0;
        const char* pointer = uenum_next(enumeration.get(), &length, &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to enumerate available collations"_s);
            return { };
        }
        String collation(unsafeMakeSpan(pointer, static_cast<size_t>(length)));
        if (collation == "standard"_s || collation == "search"_s)
            continue;
        if (auto mapped = mapICUCollationKeywordToBCP47(collation))
            elements.append(WTFMove(mapped.value()));
        else
            elements.append(WTFMove(collation));
    }

    // The AvailableCollations abstract operation returns a List, ordered as if an Array of the same
    // values had been sorted using %Array.prototype.sort% using undefined as comparator
    std::sort(elements.begin(), elements.end(),
        [](const String& a, const String& b) {
            return WTF::codePointCompare(a, b) < 0;
        });
    auto end = std::unique(elements.begin(), elements.end());
    elements.shrink(elements.size() - (elements.end() - end));

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

// https://tc39.es/proposal-intl-enumeration/#sec-availablecurrencies
static JSArray* availableCurrencies(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucurr_openISOCurrencies(UCURR_ALL, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available currencies"_s);
        return { };
    }

    int32_t count = uenum_count(enumeration.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available currencies"_s);
        return { };
    }

    Vector<String, 1> elements;
    // ICU ~69 doesn't list VES and UYW, but it is actually supported via Intl.DisplayNames.
    // And ICU ~69 lists up EQE / LSM while it cannot return information via Intl.DisplayNames.
    // So, we need to add the following work-around.
    //     1. Add VES and UYW
    //     2. Do not add EQE and LSM
    // https://unicode-org.atlassian.net/browse/ICU-21685
    elements.reserveInitialCapacity(count + 2);
    elements.append("VES"_s);
    elements.append("UYW"_s);
    for (int32_t index = 0; index < count; ++index) {
        int32_t length = 0;
        const char* pointer = uenum_next(enumeration.get(), &length, &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to enumerate available currencies"_s);
            return { };
        }
        String currency(unsafeMakeSpan(pointer, static_cast<size_t>(length)));
        if (currency == "EQE"_s)
            continue;
        if (currency == "LSM"_s)
            continue;
        elements.append(WTFMove(currency));
    }

    // The AvailableCurrencies abstract operation returns a List, ordered as if an Array of the same
    // values had been sorted using %Array.prototype.sort% using undefined as comparator
    std::sort(elements.begin(), elements.end(),
        [](const String& a, const String& b) {
            return WTF::codePointCompare(a, b) < 0;
        });
    auto end = std::unique(elements.begin(), elements.end());
    elements.shrink(elements.size() - (elements.end() - end));

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

// https://tc39.es/proposal-intl-enumeration/#sec-availablenumberingsystems
static JSArray* availableNumberingSystems(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(unumsys_openAvailableNames(&status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available numbering systems"_s);
        return { };
    }

    int32_t count = uenum_count(enumeration.get(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to enumerate available numbering systems"_s);
        return { };
    }

    Vector<String, 1> elements;
    elements.reserveInitialCapacity(count);
    for (int32_t index = 0; index < count; ++index) {
        int32_t length = 0;
        const char* name = uenum_next(enumeration.get(), &length, &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to enumerate available numbering systems"_s);
            return { };
        }
        auto numberingSystem = std::unique_ptr<UNumberingSystem, ICUDeleter<unumsys_close>>(unumsys_openByName(name, &status));
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to enumerate available numbering systems"_s);
            return { };
        }
        if (unumsys_isAlgorithmic(numberingSystem.get()))
            continue;
        elements.constructAndAppend(std::span { name, static_cast<size_t>(length) });
    }

    // The AvailableNumberingSystems abstract operation returns a List, ordered as if an Array of the same
    // values had been sorted using %Array.prototype.sort% using undefined as comprator
    std::sort(elements.begin(), elements.end(),
        [](const String& a, const String& b) {
            return WTF::codePointCompare(a, b) < 0;
        });

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

static bool isValidTimeZoneNameFromICUTimeZone(StringView timeZoneName)
{
    // Some time zone names are included in ICU, but they are not included in the IANA Time Zone Database.
    // We need to filter them out.
    if (timeZoneName.startsWith("SystemV/"_s))
        return false;
    if (timeZoneName.startsWith("Etc/"_s))
        return true;
    // IANA time zone names include '/'. Some of them are not including, but it is in backward links.
    // And ICU already resolved these backward links.
    if (!timeZoneName.contains('/'))
        return timeZoneName == "UTC"_s || timeZoneName == "GMT"_s;
    return true;
}

// https://tc39.es/proposal-intl-enumeration/#sec-canonicalizetimezonename
static std::optional<String> canonicalizeTimeZoneNameFromICUTimeZone(String&& timeZoneName)
{
    if (isUTCEquivalent(timeZoneName))
        return "UTC"_s;
    return std::make_optional(WTFMove(timeZoneName));
}

// https://tc39.es/ecma402/#sup-availablenamedtimezoneidentifiers
const Vector<String>& intlAvailableTimeZones()
{
    static LazyNeverDestroyed<Vector<String>> availableTimeZones;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        Vector<String> temporary;
        UErrorCode status = U_ZERO_ERROR;
        auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, nullptr, nullptr, &status));
        ASSERT(U_SUCCESS(status));

        int32_t count = uenum_count(enumeration.get(), &status);
        ASSERT(U_SUCCESS(status));
        temporary.reserveInitialCapacity(count);
        for (int32_t index = 0; index < count; ++index) {
            int32_t length = 0;
            const char* pointer = uenum_next(enumeration.get(), &length, &status);
            ASSERT(U_SUCCESS(status));
            String timeZone(unsafeMakeSpan(pointer, static_cast<size_t>(length)));
            if (isValidTimeZoneNameFromICUTimeZone(timeZone)) {
                if (auto mapped = canonicalizeTimeZoneNameFromICUTimeZone(WTFMove(timeZone)))
                    temporary.append(WTFMove(mapped.value()));
            }
        }

        // The AvailableTimeZones abstract operation returns a List, ordered as if an Array of the same
        // values had been sorted using %Array.prototype.sort% using undefined as comparator
        std::sort(temporary.begin(), temporary.end(),
            [](const String& a, const String& b) {
                return WTF::codePointCompare(a, b) < 0;
            });
        auto end = std::unique(temporary.begin(), temporary.end());
        availableTimeZones.construct();

        auto createImmortalThreadSafeString = [&](String&& string) {
            if (string.is8Bit())
                return StringImpl::createStaticStringImpl(string.span8());
            return StringImpl::createStaticStringImpl(string.span16());
        };
        availableTimeZones.get() = WTF::map(std::span(temporary.begin(), end), [&](auto&& string) -> String {
            return createImmortalThreadSafeString(WTFMove(string));
        });
    });
    return availableTimeZones;
}

TimeZoneID utcTimeZoneIDStorage { std::numeric_limits<TimeZoneID>::max() };
TimeZoneID utcTimeZoneIDSlow()
{
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        auto& timeZones = intlAvailableTimeZones();
        auto index = timeZones.find("UTC"_s);
        RELEASE_ASSERT(index != WTF::notFound);
        utcTimeZoneIDStorage = index;
    });
    return utcTimeZoneIDStorage;
}

// https://tc39.es/ecma402/#sec-availableprimarytimezoneidentifiers
static JSArray* availablePrimaryTimeZoneIdentifiers(JSGlobalObject* globalObject)
{
    return createArrayFromStringVector(globalObject, intlAvailableTimeZones());
}

// https://tc39.es/proposal-intl-enumeration/#sec-availableunits
static JSArray* availableUnits(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), std::size(simpleUnits));
    if (!result) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    ASSERT(
        std::is_sorted(std::begin(simpleUnits), std::end(simpleUnits),
            [](const MeasureUnit& a, const MeasureUnit& b) {
                return WTF::codePointCompare(StringView(a.subType), StringView(b.subType)) < 0;
            }));

    int32_t index = 0;
    for (const MeasureUnit& unit : simpleUnits) {
        result->putDirectIndex(globalObject, index++, jsString(vm, StringImpl::create(unit.subType)));
        RETURN_IF_EXCEPTION(scope, { });
    }
    return result;
}

// https://tc39.es/ecma402/#sec-intl.supportedvaluesof
JSC_DEFINE_HOST_FUNCTION(intlObjectFuncSupportedValuesOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String key = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (key == "calendar"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availableCalendars(globalObject)));

    if (key == "collation"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availableCollations(globalObject)));

    if (key == "currency"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availableCurrencies(globalObject)));

    if (key == "numberingSystem"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availableNumberingSystems(globalObject)));

    if (key == "timeZone"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availablePrimaryTimeZoneIdentifiers(globalObject)));

    if (key == "unit"_s)
        RELEASE_AND_RETURN(scope, JSValue::encode(availableUnits(globalObject)));

    throwRangeError(globalObject, scope, "Unknown key for Intl.supportedValuesOf"_s);
    return { };
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
