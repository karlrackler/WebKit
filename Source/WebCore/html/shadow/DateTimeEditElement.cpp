/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "DateTimeEditElement.h"

#include "ContainerNodeInlines.h"
#include "CSSPropertyNames.h"
#include "DateComponents.h"
#include "DateTimeFieldElements.h"
#include "DateTimeFieldsState.h"
#include "DateTimeFormat.h"
#include "DateTimeSymbolicFieldElement.h"
#include "Document.h"
#include "Event.h"
#include "EventTargetInlines.h"
#include "ExceptionOr.h"
#include "HTMLNames.h"
#include "NodeInlines.h"
#include "PlatformLocale.h"
#include "RenderElement.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "UserAgentParts.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(DateTimeEditElement);

class DateTimeEditBuilder final : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeEditBuilder);

public:
    DateTimeEditBuilder(DateTimeEditElement&, const DateTimeEditElement::LayoutParameters&);

    bool build(const String&);

private:
    // DateTimeFormat::TokenHandler functions:
    void visitField(DateTimeFormat::FieldType, int);
    void visitLiteral(const String&);

    const Ref<DateTimeEditElement> m_editElement;
    const DateTimeEditElement::LayoutParameters& m_parameters;
};

DateTimeEditBuilder::DateTimeEditBuilder(DateTimeEditElement& element, const DateTimeEditElement::LayoutParameters& layoutParameters)
    : m_editElement(element)
    , m_parameters(layoutParameters)
{
}

bool DateTimeEditBuilder::build(const String& formatString)
{
    m_editElement->resetFields();
    return DateTimeFormat::parse(formatString, *this);
}

void DateTimeEditBuilder::visitField(DateTimeFormat::FieldType fieldType, int count)
{
    Ref document = m_editElement->document();

    switch (fieldType) {
    case DateTimeFormat::FieldTypeDayOfMonth: {
        m_editElement->addField(DateTimeDayFieldElement::create(document.get(), m_editElement));
        return;
    }

    case DateTimeFormat::FieldTypeFractionalSecond: {
        m_editElement->addField(DateTimeMillisecondFieldElement::create(document.get(), m_editElement));
        return;
    }

    case DateTimeFormat::FieldTypeHour11: {
        m_editElement->addField(DateTimeHourFieldElement::create(document.get(), m_editElement, 0, 11));
        return;
    }

    case DateTimeFormat::FieldTypeHour12: {
        m_editElement->addField(DateTimeHourFieldElement::create(document.get(), m_editElement, 1, 12));
        return;
    }

    case DateTimeFormat::FieldTypeHour23: {
        m_editElement->addField(DateTimeHourFieldElement::create(document.get(), m_editElement, 0, 23));
        return;
    }

    case DateTimeFormat::FieldTypeHour24: {
        m_editElement->addField(DateTimeHourFieldElement::create(document.get(), m_editElement, 1, 24));
        return;
    }

    case DateTimeFormat::FieldTypeMinute: {
        m_editElement->addField(DateTimeMinuteFieldElement::create(document.get(), m_editElement));
        return;
    }

    case DateTimeFormat::FieldTypeMonth:
    case DateTimeFormat::FieldTypeMonthStandAlone: {
        constexpr int countForAbbreviatedMonth = 3;
        constexpr int countForFullMonth = 4;
        constexpr int countForNarrowMonth = 5;

        switch (count) {
        case countForNarrowMonth:
        case countForAbbreviatedMonth: {
            Ref field = DateTimeSymbolicMonthFieldElement::create(document.get(), m_editElement, fieldType == DateTimeFormat::FieldTypeMonth ? m_parameters.locale.shortMonthLabels() : m_parameters.locale.shortStandAloneMonthLabels());
            m_editElement->addField(field);
            return;
        }
        case countForFullMonth: {
            Ref field = DateTimeSymbolicMonthFieldElement::create(document.get(), m_editElement, fieldType == DateTimeFormat::FieldTypeMonth ? m_parameters.locale.monthLabels() : m_parameters.locale.standAloneMonthLabels());
            m_editElement->addField(field);
            return;
        }
        default:
            m_editElement->addField(DateTimeMonthFieldElement::create(document.get(), m_editElement));
            return;
        }
    }

    case DateTimeFormat::FieldTypePeriod: {
        m_editElement->addField(DateTimeMeridiemFieldElement::create(document.get(), m_editElement, m_parameters.locale.timeAMPMLabels()));
        return;
    }

    case DateTimeFormat::FieldTypeSecond: {
        m_editElement->addField(DateTimeSecondFieldElement::create(document.get(), m_editElement));

        if (m_parameters.shouldHaveMillisecondField) {
            visitLiteral(m_parameters.locale.localizedDecimalSeparator());
            visitField(DateTimeFormat::FieldTypeFractionalSecond, 3);
        }
        return;
    }

    case DateTimeFormat::FieldTypeYear: {
        m_editElement->addField(DateTimeYearFieldElement::create(document.get(), m_editElement));
        return;
    }

    default:
        return;
    }
}

void DateTimeEditBuilder::visitLiteral(const String& text)
{
    ASSERT(text.length());

    Ref document = m_editElement->document();
    Ref element = HTMLDivElement::create(document.get());
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditText());

    // If the literal begins/ends with a space, the gap between two fields will appear
    // exaggerated due to the presence of a 1px padding around each field. This can
    // make spaces appear up to 2px larger between fields. This padding is necessary to
    // prevent selected fields from appearing squished. To fix, pull fields closer
    // together by applying a negative margin.
    if (text.startsWith(' '))
        element->setInlineStyleProperty(CSSPropertyMarginInlineStart, -1, CSSUnitType::CSS_PX);
    if (text.endsWith(' '))
        element->setInlineStyleProperty(CSSPropertyMarginInlineEnd, -1, CSSUnitType::CSS_PX);

    element->appendChild(Text::create(document.get(), String { text }));
    m_editElement->protectedFieldsWrapperElement()->appendChild(element);
}

DateTimeEditElementEditControlOwner::~DateTimeEditElementEditControlOwner() = default;

DateTimeEditElement::DateTimeEditElement(Document& document, DateTimeEditElementEditControlOwner& editControlOwner)
    : HTMLDivElement(divTag, document)
    , m_editControlOwner(editControlOwner)
{
    m_placeholderDate.setToCurrentLocalTime();
}

DateTimeEditElement::~DateTimeEditElement() = default;

inline Element& DateTimeEditElement::fieldsWrapperElement() const
{
    ASSERT(firstChild());
    return downcast<Element>(*firstChild());
}

inline Ref<Element> DateTimeEditElement::protectedFieldsWrapperElement() const
{
    return fieldsWrapperElement();
}

void DateTimeEditElement::addField(Ref<DateTimeFieldElement> field)
{
    if (m_fields.size() == m_fields.capacity())
        return;
    m_fields.append(field);
    protectedFieldsWrapperElement()->appendChild(field);
}

size_t DateTimeEditElement::fieldIndexOf(const DateTimeFieldElement& fieldToFind) const
{
    return m_fields.findIf([&] (auto& field) {
        return field.ptr() == &fieldToFind;
    });
}

DateTimeFieldElement* DateTimeEditElement::focusedFieldElement() const
{
    RefPtr focusedElement = document().focusedElement();
    auto fieldIndex = m_fields.findIf([&] (auto& field) {
        return field.ptr() == focusedElement.get();
    });

    if (fieldIndex == notFound)
        return nullptr;

    return m_fields[fieldIndex].ptr();
}

Ref<DateTimeEditElement> DateTimeEditElement::create(Document& document, DateTimeEditElementEditControlOwner& editControlOwner)
{
    Ref element = adoptRef(*new DateTimeEditElement(document, editControlOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEdit());
    return element;
}

void DateTimeEditElement::layout(const LayoutParameters& layoutParameters)
{
    if (!firstChild()) {
        Ref element = HTMLDivElement::create(protectedDocument().get());
        appendChild(element);
        element->setUserAgentPart(UserAgentParts::webkitDatetimeEditFieldsWrapper());
    }

    Ref fieldsWrapper = fieldsWrapperElement();
    RefPtr focusedField = focusedFieldElement();

    DateTimeEditBuilder builder(*this, layoutParameters);
    RefPtr lastChildToBeRemoved = fieldsWrapper->lastChild();
    if (!builder.build(layoutParameters.dateTimeFormat) || m_fields.isEmpty()) {
        lastChildToBeRemoved = fieldsWrapper->lastChild();
        builder.build(layoutParameters.fallbackDateTimeFormat);
    }

    if (focusedField) {
        auto& focusedFieldId = focusedField->userAgentPart();

        auto foundFieldToFocus = false;
        for (auto& field : m_fields) {
            if (field->userAgentPart() == focusedFieldId) {
                foundFieldToFocus = true;
                field->focus();
                break;
            }
        }

        if (!foundFieldToFocus)
            focusOnNextFocusableField(0);
    }

    if (lastChildToBeRemoved) {
        while (RefPtr childNode = fieldsWrapper->firstChild()) {
            fieldsWrapper->removeChild(*childNode);
            if (childNode == lastChildToBeRemoved)
                break;
        }
    }
}

void DateTimeEditElement::didBlurFromField(Event& event)
{
    if (!m_editControlOwner)
        return;

    if (RefPtr newFocusedElement = event.relatedTarget()) {
        bool didFocusSiblingField = notFound != m_fields.findIf([&] (auto& field) {
            return field.ptr() == newFocusedElement.get();
        });

        if (didFocusSiblingField)
            return;
    }

    m_editControlOwner->didBlurFromControl();
}

void DateTimeEditElement::fieldValueChanged()
{
    if (m_editControlOwner)
        m_editControlOwner->didChangeValueFromControl();
}

bool DateTimeEditElement::focusOnNextFocusableField(size_t startIndex)
{
    for (size_t i = startIndex; i < m_fields.size(); ++i) {
        Ref field = m_fields[i];
        if (field->isFocusable()) {
            field->focus();
            return true;
        }
    }
    return false;
}

void DateTimeEditElement::focusByOwner()
{
    focusOnNextFocusableField(0);
}

bool DateTimeEditElement::focusOnNextField(const DateTimeFieldElement& field)
{
    auto startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == notFound)
        return false;

    return focusOnNextFocusableField(startFieldIndex + 1);
}

bool DateTimeEditElement::focusOnPreviousField(const DateTimeFieldElement& field)
{
    auto startFieldIndex = fieldIndexOf(field);
    if (startFieldIndex == notFound)
        return false;

    auto fieldIndex = startFieldIndex;
    while (fieldIndex > 0) {
        --fieldIndex;
        Ref field = m_fields[fieldIndex];
        if (field->isFocusable()) {
            field->focus();
            return true;
        }
    }

    return false;
}

bool DateTimeEditElement::isFieldOwnerDisabled() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerDisabled();
}

bool DateTimeEditElement::isFieldOwnerReadOnly() const
{
    return m_editControlOwner && m_editControlOwner->isEditControlOwnerReadOnly();
}

bool DateTimeEditElement::isFieldOwnerHorizontal() const
{
    if (CheckedPtr renderer = fieldsWrapperElement().renderer())
        return renderer->isHorizontalWritingMode();
    return true;
}

AtomString DateTimeEditElement::localeIdentifier() const
{
    return m_editControlOwner ? m_editControlOwner->localeIdentifier() : nullAtom();
}

const GregorianDateTime& DateTimeEditElement::placeholderDate() const
{
    return m_placeholderDate;
}

void DateTimeEditElement::resetFields()
{
    m_fields.shrink(0);
}

void DateTimeEditElement::setValueAsDate(const LayoutParameters& layoutParameters, const DateComponents& date)
{
    layout(layoutParameters);
    for (auto& field : m_fields)
        field->setValueAsDate(date);
}

void DateTimeEditElement::setEmptyValue(const LayoutParameters& layoutParameters)
{
    layout(layoutParameters);
    for (auto& field : m_fields)
        field->setEmptyValue();
}

String DateTimeEditElement::value() const
{
    return m_editControlOwner ? m_editControlOwner->formatDateTimeFieldsState(valueAsDateTimeFieldsState()) : emptyString();
}

String DateTimeEditElement::placeholderValue() const
{
    return m_editControlOwner ? m_editControlOwner->formatDateTimeFieldsState(valueAsDateTimeFieldsState(DateTimePlaceholderIfNoValue::Yes)) : emptyString();
}

DateTimeFieldsState DateTimeEditElement::valueAsDateTimeFieldsState(DateTimePlaceholderIfNoValue placeholderIfNoValue) const
{
    DateTimeFieldsState dateTimeFieldsState;
    for (auto& field : m_fields)
        field->populateDateTimeFieldsState(dateTimeFieldsState, placeholderIfNoValue);
    return dateTimeFieldsState;
}

bool DateTimeEditElement::editableFieldsHaveValues() const
{
    for (auto& field : m_fields) {
        if (field->hasValue())
            return true;
    }
    return false;
}

} // namespace WebCore
