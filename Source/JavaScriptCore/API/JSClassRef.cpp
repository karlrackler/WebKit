/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSClassRef.h"

#include "APICast.h"
#include "InitializeThreading.h"
#include "JSCInlines.h"
#include "JSCallbackObject.h"

using namespace JSC;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

const JSClassDefinition kJSClassDefinitionEmpty = { 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

OpaqueJSClass::OpaqueJSClass(const JSClassDefinition* definition, OpaqueJSClass* protoClass) 
    : parentClass(definition->parentClass)
    , prototypeClass(nullptr)
    , initialize(definition->initialize)
    , finalize(definition->finalize)
    , hasProperty(definition->hasProperty)
    , getProperty(definition->getProperty)
    , setProperty(definition->setProperty)
    , deleteProperty(definition->deleteProperty)
    , getPropertyNames(definition->getPropertyNames)
    , callAsFunction(definition->callAsFunction)
    , callAsConstructor(definition->callAsConstructor)
    , hasInstance(definition->hasInstance)
    , convertToType(definition->convertToType)
    , m_className(String::fromUTF8(definition->className))
{
    JSC::initialize();

    if (const JSStaticValue* staticValue = definition->staticValues) {
        while (staticValue->name) {
            String valueName = String::fromUTF8(staticValue->name);
            if (!valueName.isNull())
                m_staticValues.add(valueName.impl(), makeUnique<StaticValueEntry>(staticValue->getProperty, staticValue->setProperty, staticValue->attributes, valueName));
            ++staticValue;
        }
    }

    if (const JSStaticFunction* staticFunction = definition->staticFunctions) {
        while (staticFunction->name) {
            String functionName = String::fromUTF8(staticFunction->name);
            if (!functionName.isNull())
                m_staticFunctions.add(functionName.impl(), makeUnique<StaticFunctionEntry>(staticFunction->callAsFunction, staticFunction->attributes));
            ++staticFunction;
        }
    }
        
    if (protoClass)
        prototypeClass = JSClassRetain(protoClass);
}

OpaqueJSClass::~OpaqueJSClass()
{
    // The empty string is shared across threads & is an identifier, in all other cases we should have done a deep copy in className(), below. 
    ASSERT(!m_className.length() || !m_className.impl()->isAtom());

#if ASSERT_ENABLED
    for (auto& key : m_staticValues.keys())
        ASSERT(!key->isAtom());
    for (auto& key : m_staticFunctions.keys())
        ASSERT(!key->isAtom());
#endif

    if (prototypeClass)
        JSClassRelease(prototypeClass);
}

Ref<OpaqueJSClass> OpaqueJSClass::createNoAutomaticPrototype(const JSClassDefinition* definition)
{
    return adoptRef(*new OpaqueJSClass(definition, nullptr));
}

Ref<OpaqueJSClass> OpaqueJSClass::create(const JSClassDefinition* clientDefinition)
{
    JSClassDefinition definition = *clientDefinition; // Avoid modifying client copy.

    JSClassDefinition protoDefinition = kJSClassDefinitionEmpty;
    protoDefinition.finalize = nullptr;
    std::swap(definition.staticFunctions, protoDefinition.staticFunctions); // Move static functions to the prototype.
    
    // We are supposed to use JSClassRetain/Release but since we know that we currently have
    // the only reference to this class object we cheat and use a RefPtr instead.
    RefPtr<OpaqueJSClass> protoClass = adoptRef(new OpaqueJSClass(&protoDefinition, nullptr));
    return adoptRef(*new OpaqueJSClass(&definition, protoClass.get()));
}

OpaqueJSClassContextData::OpaqueJSClassContextData(JSC::VM&, OpaqueJSClass* jsClass)
    : m_class(jsClass)
{
    for (auto& it : jsClass->m_staticValues) {
        ASSERT(!it.key->isAtom());
        String valueName = it.key->isolatedCopy();
        staticValues.add(valueName.impl(), makeUnique<StaticValueEntry>(it.value->getProperty, it.value->setProperty, it.value->attributes, valueName));
    }

    for (auto& it : jsClass->m_staticFunctions) {
        ASSERT(!it.key->isAtom());
        staticFunctions.add(it.key->isolatedCopy(), makeUnique<StaticFunctionEntry>(it.value->callAsFunction, it.value->attributes));
    }
}

OpaqueJSClassContextData& OpaqueJSClass::contextData(JSGlobalObject* globalObject)
{
    auto& contextData = globalObject->contextData(this);
    if (!contextData)
        contextData = makeUnique<OpaqueJSClassContextData>(globalObject->vm(), this);
    return *contextData;
}

String OpaqueJSClass::className()
{
    // Make a deep copy, so that the caller has no chance to put the original into AtomStringTable.
    return m_className.isolatedCopy();
}

OpaqueJSClassStaticValuesTable* OpaqueJSClass::staticValues(JSC::JSGlobalObject* globalObject)
{
    return &contextData(globalObject).staticValues;
}

OpaqueJSClassStaticFunctionsTable* OpaqueJSClass::staticFunctions(JSC::JSGlobalObject* globalObject)
{
    return &contextData(globalObject).staticFunctions;
}

JSObject* OpaqueJSClass::prototype(JSGlobalObject* globalObject)
{
    /* Class (C++) and prototype (JS) inheritance are parallel, so:
     *     (C++)      |        (JS)
     *   ParentClass  |   ParentClassPrototype
     *       ^        |          ^
     *       |        |          |
     *  DerivedClass  |  DerivedClassPrototype
     */

    if (!prototypeClass)
        return nullptr;

    OpaqueJSClassContextData& jsClassData = contextData(globalObject);

    if (JSObject* prototype = jsClassData.cachedPrototype.get())
        return prototype;

    // Recursive, but should be good enough for our purposes
    JSObject* prototype = JSCallbackObject<JSNonFinalObject>::create(globalObject, globalObject->callbackObjectStructure(), prototypeClass, &jsClassData); // set jsClassData as the object's private data, so it can clear our reference on destruction
    if (parentClass) {
        if (JSObject* parentPrototype = parentClass->prototype(globalObject))
            prototype->setPrototypeDirect(globalObject->vm(), parentPrototype);
    }

    jsClassData.cachedPrototype = Weak<JSObject>(prototype);
    return prototype;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
