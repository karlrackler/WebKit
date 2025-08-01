/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInjectedScriptHost.h"

#include "CachedHTMLCollectionInlines.h"
#include "DOMException.h"
#include "JSDOMException.h"
#include "JSEventListener.h"
#include "JSEventTarget.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "JSWorker.h"
#include "Worker.h"

#if ENABLE(PAYMENT_REQUEST)
#include "JSPaymentRequest.h"
#include "JSPaymentShippingType.h"
#include "PaymentOptions.h"
#include "PaymentRequest.h"
#endif

namespace WebCore {

using namespace JSC;

JSValue WebInjectedScriptHost::subtype(JSGlobalObject* exec, JSValue value)
{
    VM& vm = exec->vm();
    if (value.inherits<JSNode>())
        return jsNontrivialString(vm, "node"_s);
    if (value.inherits<JSNodeList>())
        return jsNontrivialString(vm, "array"_s);
    if (value.inherits<JSHTMLCollection>())
        return jsNontrivialString(vm, "array"_s);
    if (value.inherits<JSDOMException>())
        return jsNontrivialString(vm, "error"_s);

    return jsUndefined();
}

static JSObject* constructInternalProperty(VM& vm, JSGlobalObject* exec, const String& name, JSValue value)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "name"_s), jsString(vm, name));
    object->putDirect(vm, Identifier::fromString(vm, "value"_s), value);
    return object;
}

#if ENABLE(PAYMENT_REQUEST)
static JSObject* objectForPaymentOptions(VM& vm, JSGlobalObject* exec, const PaymentOptions& paymentOptions)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerName"_s), jsBoolean(paymentOptions.requestPayerName));
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerEmail"_s), jsBoolean(paymentOptions.requestPayerEmail));
    object->putDirect(vm, Identifier::fromString(vm, "requestPayerPhone"_s), jsBoolean(paymentOptions.requestPayerPhone));
    object->putDirect(vm, Identifier::fromString(vm, "requestShipping"_s), jsBoolean(paymentOptions.requestShipping));
    object->putDirect(vm, Identifier::fromString(vm, "shippingType"_s), jsNontrivialString(vm, convertEnumerationToString(paymentOptions.shippingType)));
    return object;
}

static JSObject* objectForPaymentCurrencyAmount(VM& vm, JSGlobalObject* exec, const PaymentCurrencyAmount& paymentCurrencyAmount)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "currency"_s), jsString(vm, paymentCurrencyAmount.currency));
    object->putDirect(vm, Identifier::fromString(vm, "value"_s), jsString(vm, paymentCurrencyAmount.value));
    return object;
}

static JSObject* objectForPaymentItem(VM& vm, JSGlobalObject* exec, const PaymentItem& paymentItem)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "label"_s), jsString(vm, paymentItem.label));
    object->putDirect(vm, Identifier::fromString(vm, "amount"_s), objectForPaymentCurrencyAmount(vm, exec, paymentItem.amount));
    object->putDirect(vm, Identifier::fromString(vm, "pending"_s), jsBoolean(paymentItem.pending));
    return object;
}

static JSObject* objectForPaymentShippingOption(VM& vm, JSGlobalObject* exec, const PaymentShippingOption& paymentShippingOption)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "id"_s), jsString(vm, paymentShippingOption.id));
    object->putDirect(vm, Identifier::fromString(vm, "label"_s), jsString(vm, paymentShippingOption.label));
    object->putDirect(vm, Identifier::fromString(vm, "amount"_s), objectForPaymentCurrencyAmount(vm, exec, paymentShippingOption.amount));
    object->putDirect(vm, Identifier::fromString(vm, "selected"_s), jsBoolean(paymentShippingOption.selected));
    return object;
}

static JSObject* objectForPaymentDetailsModifier(VM& vm, JSGlobalObject* exec, const PaymentDetailsModifier& modifier)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "supportedMethods"_s), jsString(vm, modifier.supportedMethods));
    if (modifier.total)
        object->putDirect(vm, Identifier::fromString(vm, "total"_s), objectForPaymentItem(vm, exec, *modifier.total));
    if (!modifier.additionalDisplayItems.isEmpty()) {
        auto* additionalDisplayItems = constructEmptyArray(exec, nullptr);
        for (unsigned i = 0; i < modifier.additionalDisplayItems.size(); ++i)
            additionalDisplayItems->putDirectIndex(exec, i, objectForPaymentItem(vm, exec, modifier.additionalDisplayItems[i]));
        object->putDirect(vm, Identifier::fromString(vm, "additionalDisplayItems"_s), additionalDisplayItems);
    }
    return object;
}

static JSObject* objectForPaymentDetails(VM& vm, JSGlobalObject* exec, const PaymentDetailsInit& paymentDetails)
{
    auto* object = constructEmptyObject(exec);
    object->putDirect(vm, Identifier::fromString(vm, "id"_s), jsString(vm, paymentDetails.id));
    object->putDirect(vm, Identifier::fromString(vm, "total"_s), objectForPaymentItem(vm, exec, paymentDetails.total));
    if (paymentDetails.displayItems) {
        auto* displayItems = constructEmptyArray(exec, nullptr);
        for (unsigned i = 0; i < paymentDetails.displayItems->size(); ++i)
            displayItems->putDirectIndex(exec, i, objectForPaymentItem(vm, exec, paymentDetails.displayItems->at(i)));
        object->putDirect(vm, Identifier::fromString(vm, "displayItems"_s), displayItems);
    }
    if (paymentDetails.shippingOptions) {
        auto* shippingOptions = constructEmptyArray(exec, nullptr);
        for (unsigned i = 0; i < paymentDetails.shippingOptions->size(); ++i)
            shippingOptions->putDirectIndex(exec, i, objectForPaymentShippingOption(vm, exec, paymentDetails.shippingOptions->at(i)));
        object->putDirect(vm, Identifier::fromString(vm, "shippingOptions"_s), shippingOptions);
    }
    if (paymentDetails.modifiers) {
        auto* modifiers = constructEmptyArray(exec, nullptr);
        for (unsigned i = 0; i < paymentDetails.modifiers->size(); ++i)
            modifiers->putDirectIndex(exec, i, objectForPaymentDetailsModifier(vm, exec, paymentDetails.modifiers->at(i)));
        object->putDirect(vm, Identifier::fromString(vm, "modifiers"_s), modifiers);
    }
    return object;
}

static JSString* jsStringForPaymentRequestState(VM& vm, PaymentRequest::State state)
{
    switch (state) {
    case PaymentRequest::State::Created:
        return jsNontrivialString(vm, "created"_s);
    case PaymentRequest::State::Interactive:
        return jsNontrivialString(vm, "interactive"_s);
    case PaymentRequest::State::Closed:
        return jsNontrivialString(vm, "closed"_s);
    }

    ASSERT_NOT_REACHED();
    return jsEmptyString(vm);
}
#endif

static JSObject* objectForEventTargetListeners(VM& vm, JSGlobalObject* exec, EventTarget* eventTarget)
{
    RefPtr scriptExecutionContext = eventTarget->scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;

    JSObject* listeners = nullptr;

    for (auto& eventType : eventTarget->eventTypes()) {
        unsigned listenersForEventIndex = 0;
        auto* listenersForEvent = constructEmptyArray(exec, nullptr);

        for (auto& eventListener : eventTarget->eventListeners(eventType)) {
            if (!is<JSEventListener>(eventListener->callback()))
                continue;

            auto& jsListener = downcast<JSEventListener>(eventListener->callback());
            if (jsListener.isolatedWorld() != &currentWorld(*exec))
                continue;

            auto* jsFunction = jsListener.ensureJSFunction(*scriptExecutionContext);
            if (!jsFunction)
                continue;

            auto* propertiesForListener = constructEmptyObject(exec);
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "callback"_s), jsFunction);
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "capture"_s), jsBoolean(eventListener->useCapture()));
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "passive"_s), jsBoolean(eventListener->isPassive()));
            propertiesForListener->putDirect(vm, Identifier::fromString(vm, "once"_s), jsBoolean(eventListener->isOnce()));
            listenersForEvent->putDirectIndex(exec, listenersForEventIndex++, propertiesForListener);
        }

        if (listenersForEventIndex) {
            if (!listeners)
                listeners = constructEmptyObject(exec);
            listeners->putDirect(vm, Identifier::fromString(vm, eventType), listenersForEvent);
        }
    }

    return listeners;
}

JSValue WebInjectedScriptHost::getInternalProperties(VM& vm, JSGlobalObject* exec, JSC::JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (RefPtr worker = JSWorker::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        String name = worker->name();
        if (!name.isEmpty())
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "name"_s, jsString(vm, WTFMove(name))));

        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "terminated"_s, jsBoolean(worker->wasTerminated())));

        if (auto* listeners = objectForEventTargetListeners(vm, exec, worker.get()))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }

#if ENABLE(PAYMENT_REQUEST)
    if (RefPtr paymentRequest = JSPaymentRequest::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "options"_s, objectForPaymentOptions(vm, exec, paymentRequest->paymentOptions())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "details"_s, objectForPaymentDetails(vm, exec, paymentRequest->paymentDetails())));
        array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "state"_s, jsStringForPaymentRequestState(vm, paymentRequest->state())));

        if (auto* listeners = objectForEventTargetListeners(vm, exec, paymentRequest.get()))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }
#endif

    if (RefPtr eventTarget = JSEventTarget::toWrapped(vm, value)) {
        unsigned index = 0;
        auto* array = constructEmptyArray(exec, nullptr);

        if (auto* listeners = objectForEventTargetListeners(vm, exec, eventTarget.get()))
            array->putDirectIndex(exec, index++, constructInternalProperty(vm, exec, "listeners"_s, listeners));

        RETURN_IF_EXCEPTION(scope, { });
        return array;
    }

    return { };
}

bool WebInjectedScriptHost::isHTMLAllCollection(JSC::VM&, JSC::JSValue value)
{
    return value.inherits<JSHTMLAllCollection>();
}

} // namespace WebCore
