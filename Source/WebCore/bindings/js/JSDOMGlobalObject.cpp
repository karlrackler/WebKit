/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "JSDOMGlobalObject.h"

#include "DOMConstructors.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FetchResponse.h"
#include "FrameDestructionObserverInlines.h"
#include "InternalWritableStream.h"
#include "JSAbortAlgorithm.h"
#include "JSAbortSignal.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSFetchResponse.h"
#include "JSIDBSerializationGlobalObject.h"
#include "JSMediaStream.h"
#include "JSMediaStreamTrack.h"
#include "JSRTCIceCandidate.h"
#include "JSRTCSessionDescription.h"
#include "JSReadableStream.h"
#include "JSShadowRealmGlobalScope.h"
#include "JSShadowRealmGlobalScopeBase.h"
#include "JSTrustedScript.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include "JSWritableStream.h"
#include "LocalDOMWindow.h"
#include "ProcessIdentifier.h"
#include "RejectedPromiseTracker.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "ServiceWorkerGlobalScope.h"
#include "ShadowRealmGlobalScope.h"
#include "SharedWorkerGlobalScope.h"
#include "StructuredClone.h"
#include "WebCoreJSBuiltinInternals.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/ConsoleClient.h>
#include <JavaScriptCore/GetterSetter.h>
#include <JavaScriptCore/GlobalObjectMethodTable.h>
#include <JavaScriptCore/JSCustomGetterFunction.h>
#include <JavaScriptCore/JSCustomSetterFunction.h>
#include <JavaScriptCore/JSGlobalProxyInlines.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/StructureInlines.h>
#include <JavaScriptCore/VMEntryScopeInlines.h>
#include <JavaScriptCore/VMTrapsInlines.h>
#include <JavaScriptCore/WasmStreamingCompiler.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <wtf/text/MakeString.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/JSRemoteInspector.h>
#endif

namespace WebCore {
using namespace JSC;

JSC_DECLARE_HOST_FUNCTION(makeThisTypeErrorForBuiltins);
JSC_DECLARE_HOST_FUNCTION(makeGetterTypeErrorForBuiltins);
JSC_DECLARE_HOST_FUNCTION(makeDOMExceptionForBuiltins);
JSC_DECLARE_HOST_FUNCTION(isReadableByteStreamAPIEnabled);
JSC_DECLARE_HOST_FUNCTION(createWritableStreamFromInternal);
JSC_DECLARE_HOST_FUNCTION(getGlobalObject);
JSC_DECLARE_HOST_FUNCTION(getInternalReadableStream);
JSC_DECLARE_HOST_FUNCTION(getInternalWritableStream);
JSC_DECLARE_HOST_FUNCTION(addAbortAlgorithmToSignal);
JSC_DECLARE_HOST_FUNCTION(removeAbortAlgorithmFromSignal);
JSC_DECLARE_HOST_FUNCTION(signalAbort);
JSC_DECLARE_HOST_FUNCTION(isAbortSignal);
JSC_DECLARE_HOST_FUNCTION(createAbortSignal);

const ClassInfo JSDOMGlobalObject::s_info = { "DOMGlobalObject"_s, &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMGlobalObject) };

JSDOMGlobalObject::JSDOMGlobalObject(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& world, const GlobalObjectMethodTable* globalObjectMethodTable)
    : JSGlobalObject(vm, structure, globalObjectMethodTable)
    , m_constructors(makeUniqueRef<DOMConstructors>())
    , m_world(WTFMove(world))
    , m_worldIsNormal(m_world->isNormal())
    , m_builtinInternalFunctions(makeUniqueRefWithoutFastMallocCheck<JSBuiltinInternalFunctions>(vm))
    , m_crossOriginFunctionMap(vm)
    , m_crossOriginGetterSetterMap(vm)
{
}

JSDOMGlobalObject::~JSDOMGlobalObject() = default;

void JSDOMGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSDOMGlobalObject*>(cell)->JSDOMGlobalObject::~JSDOMGlobalObject();
}

JSC_DEFINE_HOST_FUNCTION(makeThisTypeErrorForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto functionName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();
    return JSValue::encode(createTypeError(globalObject, makeThisTypeErrorMessage(interfaceName.utf8().data(), functionName.utf8().data())));
}

JSC_DEFINE_HOST_FUNCTION(makeGetterTypeErrorForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto attributeName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();

    auto error = static_cast<ErrorInstance*>(createTypeError(globalObject, JSC::makeDOMAttributeGetterTypeErrorMessage(interfaceName.utf8().data(), attributeName)));
    error->setNativeGetterTypeError();
    return JSValue::encode(error);
}

JSC_DEFINE_HOST_FUNCTION(makeDOMExceptionForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto codeValue = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();

    auto message = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();

    ExceptionCode code { ExceptionCode::TypeError };
    if (codeValue == "AbortError"_s)
        code = ExceptionCode::AbortError;
    auto value = createDOMException(globalObject, code, message);

    EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException());

    return JSValue::encode(value);
}

JSC_DEFINE_HOST_FUNCTION(isReadableByteStreamAPIEnabled, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsBoolean(DeprecatedGlobalSettings::readableByteStreamAPIEnabled()));
}

JSC_DEFINE_HOST_FUNCTION(getInternalWritableStream, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 1);

    auto* writableStream = jsDynamicCast<JSWritableStream*>(callFrame->uncheckedArgument(0));
    if (!writableStream) [[unlikely]]
        return JSValue::encode(jsUndefined());
    return JSValue::encode(writableStream->wrapped().internalWritableStream());
}

JSC_DEFINE_HOST_FUNCTION(getGlobalObject, (JSGlobalObject* globalObject, CallFrame*))
{
    return JSValue::encode(globalObject);
}

JSC_DEFINE_HOST_FUNCTION(getInternalReadableStream, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 1);

    auto* readableStream = jsDynamicCast<JSReadableStream*>(callFrame->uncheckedArgument(0));
    if (!readableStream) [[unlikely]]
        return JSValue::encode(jsUndefined());
    return JSValue::encode(readableStream->wrapped().internalReadableStream());
}

JSC_DEFINE_HOST_FUNCTION(createWritableStreamFromInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 1);
    ASSERT(callFrame->uncheckedArgument(0).isObject());

    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    auto internalWritableStream = InternalWritableStream::fromObject(*jsDOMGlobalObject, *callFrame->uncheckedArgument(0).toObject(globalObject));
    return JSValue::encode(toJSNewlyCreated(globalObject, jsDOMGlobalObject, WritableStream::create(WTFMove(internalWritableStream))));
}

JSC_DEFINE_HOST_FUNCTION(addAbortAlgorithmToSignal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto* abortSignal = jsDynamicCast<JSAbortSignal*>(callFrame->uncheckedArgument(0));
    if (!abortSignal) [[unlikely]]
        return JSValue::encode(JSValue(JSC::JSValue::JSFalse));

    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    Ref<AbortAlgorithm> abortAlgorithm = JSAbortAlgorithm::create(callFrame->uncheckedArgument(1).getObject(), jsDOMGlobalObject);

    auto algorithmIdentifier = AbortSignal::addAbortAlgorithmToSignal(abortSignal->protectedWrapped().get(), WTFMove(abortAlgorithm));
    return JSValue::encode(JSC::jsNumber(algorithmIdentifier));
}

JSC_DEFINE_HOST_FUNCTION(removeAbortAlgorithmFromSignal, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto* abortSignal = jsDynamicCast<JSAbortSignal*>(callFrame->uncheckedArgument(0));
    if (!abortSignal) [[unlikely]]
        return JSValue::encode(JSValue(JSC::JSValue::JSFalse));

    AbortSignal::removeAbortAlgorithmFromSignal(abortSignal->protectedWrapped().get(), callFrame->uncheckedArgument(1).asUInt32());
    return JSValue::encode(JSC::jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(isAbortSignal, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame->argumentCount() == 1);
    return JSValue::encode(jsBoolean(callFrame->uncheckedArgument(0).inherits<JSAbortSignal>()));
}

JSC_DEFINE_HOST_FUNCTION(createAbortSignal, (JSGlobalObject* globalObject, CallFrame*))
{
    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    return JSValue::encode(toJS(globalObject, jsDOMGlobalObject, AbortSignal::create(jsDOMGlobalObject->scriptExecutionContext())));
}

JSC_DEFINE_HOST_FUNCTION(signalAbort, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto* abortSignal = jsDynamicCast<JSAbortSignal*>(callFrame->uncheckedArgument(0));
    if (abortSignal) [[unlikely]]
        abortSignal->protectedWrapped()->signalAbort(callFrame->uncheckedArgument(1));
    return JSValue::encode(JSC::jsUndefined());
}

SUPPRESS_ASAN void JSDOMGlobalObject::addBuiltinGlobals(VM& vm)
{
    m_builtinInternalFunctions->initialize(*this);

    auto& builtinNames = WebCore::builtinNames(vm);
    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.makeThisTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeThisTypeErrorForBuiltins, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.makeGetterTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeGetterTypeErrorForBuiltins, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.makeDOMExceptionPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeDOMExceptionForBuiltins, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.addAbortAlgorithmToSignalPrivateName(),
            JSFunction::create(vm, this, 2, String(), addAbortAlgorithmToSignal, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.removeAbortAlgorithmFromSignalPrivateName(),
            JSFunction::create(vm, this, 2, String(), removeAbortAlgorithmFromSignal, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.signalAbortPrivateName(),
            JSFunction::create(vm, this, 2, String(), signalAbort, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.createAbortSignalPrivateName(),
            JSFunction::create(vm, this, 0, String(), createAbortSignal, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.cloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 3, String(), cloneArrayBuffer, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.structuredCloneForStreamPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneForStream, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(vm.propertyNames->builtinNames().ArrayBufferPrivateName(), arrayBufferConstructor(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamClosedPrivateName(), jsNumber(1), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamClosingPrivateName(), jsNumber(2), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamErroredPrivateName(), jsNumber(3), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamReadablePrivateName(), jsNumber(4), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamWaitingPrivateName(), jsNumber(5), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.streamWritablePrivateName(), jsNumber(6), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.readableByteStreamAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isReadableByteStreamAPIEnabled, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.isAbortSignalPrivateName(), JSFunction::create(vm, this, 1, String(), isAbortSignal, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.getInternalReadableStreamPrivateName(), JSFunction::create(vm, this, 1, String(), getInternalReadableStream, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.getInternalWritableStreamPrivateName(), JSFunction::create(vm, this, 1, String(), getInternalWritableStream, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.getGlobalObjectPrivateName(), JSFunction::create(vm, this, 1, String(), getGlobalObject, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(builtinNames.createWritableStreamFromInternalPrivateName(), JSFunction::create(vm, this, 1, String(), createWritableStreamFromInternal, ImplementationVisibility::Public), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(staticGlobals, std::size(staticGlobals));
}

void JSDOMGlobalObject::finishCreation(VM& vm)
{
#if ENABLE(REMOTE_INSPECTOR)
    bool inspectionPreviouslyFollowedInternalPolicies = JSRemoteInspectorGetInspectionFollowsInternalPolicies();
    JSRemoteInspectorSetInspectionFollowsInternalPolicies(false);
#endif

    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo());

#if ENABLE(REMOTE_INSPECTOR)
    JSRemoteInspectorSetInspectionFollowsInternalPolicies(inspectionPreviouslyFollowedInternalPolicies);
#endif
}

void JSDOMGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
#if ENABLE(REMOTE_INSPECTOR)
    bool inspectionPreviouslyFollowedInternalPolicies = JSRemoteInspectorGetInspectionFollowsInternalPolicies();
    JSRemoteInspectorSetInspectionFollowsInternalPolicies(false);
#endif

    Base::finishCreation(vm, thisValue);
    ASSERT(inherits(info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo());

#if ENABLE(REMOTE_INSPECTOR)
    JSRemoteInspectorSetInspectionFollowsInternalPolicies(inspectionPreviouslyFollowedInternalPolicies);
#endif
}

RefPtr<ScriptExecutionContext> JSDOMGlobalObject::protectedScriptExecutionContext() const
{
    return scriptExecutionContext();
}

ScriptExecutionContext* JSDOMGlobalObject::scriptExecutionContext() const
{
    if (inherits<JSDOMWindowBase>())
        return jsCast<const JSDOMWindowBase*>(this)->scriptExecutionContext();
    if (inherits<JSShadowRealmGlobalScopeBase>())
        return jsCast<const JSShadowRealmGlobalScopeBase*>(this)->scriptExecutionContext();
    if (inherits<JSWorkerGlobalScopeBase>())
        return jsCast<const JSWorkerGlobalScopeBase*>(this)->scriptExecutionContext();
    if (inherits<JSWorkletGlobalScopeBase>())
        return jsCast<const JSWorkletGlobalScopeBase*>(this)->scriptExecutionContext();
    if (inherits<JSIDBSerializationGlobalObject>())
        return jsCast<const JSIDBSerializationGlobalObject*>(this)->scriptExecutionContext();

    dataLog("Unexpected global object: ", JSValue(this), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

String JSDOMGlobalObject::codeForEval(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();

    if (auto* script = JSTrustedScript::toWrapped(vm, value))
        return script->toString();

    return String();
}

bool JSDOMGlobalObject::canCompileStrings(JSGlobalObject* globalObject, CompilationType compilationType, String codeString, const ArgList& args)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto& thisObject = static_cast<JSDOMGlobalObject&>(*globalObject);
    auto* scriptExecutionContext = thisObject.scriptExecutionContext();

    auto result = canCompile(*scriptExecutionContext, compilationType, codeString, args);

    if (result.hasException()) {
        // https://w3c.github.io/webappsec-csp/#can-compile-strings
        // Step 2.7. If the algorithm throws an error, throw an EvalError.
        // This clears the existing exceptions and returns false, where the caller throws an EvalError.
        throwScope.clearException();
        return false;
    }

    return result.releaseReturnValue();
}

Structure* JSDOMGlobalObject::trustedScriptStructure(JSGlobalObject* globalObject)
{
    auto& thisObject = static_cast<JSDOMGlobalObject&>(*globalObject);

    return getDOMStructure<JSTrustedScript>(globalObject->vm(), thisObject);
}

template<typename Visitor>
void JSDOMGlobalObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSDOMGlobalObject* thisObject = jsCast<JSDOMGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    {
        // The GC thread has to grab the GC lock even though it is not mutating the containers.
        Locker locker { thisObject->m_gcLock };

        for (auto& structure : thisObject->m_structures.values())
            visitor.append(structure);

        for (auto& guarded : thisObject->m_guardedObjects)
            guarded->visitAggregate(visitor);
    }

    for (auto& constructor : thisObject->constructors().array())
        visitor.append(constructor);

    thisObject->m_builtinInternalFunctions->visit(visitor);
}

DEFINE_VISIT_CHILDREN(JSDOMGlobalObject);

void JSDOMGlobalObject::promiseRejectionTracker(JSGlobalObject* jsGlobalObject, JSPromise* promise, JSPromiseRejectionOperation operation)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto* context = globalObject.scriptExecutionContext();
    if (!context)
        return;

    auto rejectedPromiseTracker = context->ensureRejectedPromiseTracker();
    if (!rejectedPromiseTracker)
        return;

    // FIXME: If script has muted errors (cross origin), terminate these steps.
    // <https://webkit.org/b/171415> Implement the `muted-errors` property of Scripts to avoid onerror/onunhandledrejection for cross-origin scripts

    switch (operation) {
    case JSPromiseRejectionOperation::Reject:
        rejectedPromiseTracker->promiseRejected(globalObject, *promise);
        break;
    case JSPromiseRejectionOperation::Handle:
        rejectedPromiseTracker->promiseHandled(globalObject, *promise);
        break;
    }
}

void JSDOMGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* jsGlobalObject, JSC::Exception* exception)
{
    reportException(jsGlobalObject, exception);
}

void JSDOMGlobalObject::clearDOMGuardedObjects() const
{
    // No locking is necessary here since we are not directly modifying the returned container.
    // Calling JSDOMGuardedObject::clear() will however modify the guarded objects container but
    // it will grab the lock as needed.
    auto guardedObjectsCopy = guardedObjects();
    for (auto& guarded : guardedObjectsCopy)
        guarded->clear();
}

JSFunction* JSDOMGlobalObject::createCrossOriginFunction(JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, NativeFunction nativeFunction, unsigned length)
{
    auto& vm = lexicalGlobalObject->vm();
    CrossOriginMapKey key = std::make_pair(lexicalGlobalObject, nativeFunction.taggedPtr());

    // WeakGCMap::ensureValue's functor must not invoke GC since GC can modify WeakGCMap in the middle of HashMap::ensure.
    // We use DeferGC here (1) not to invoke GC when executing WeakGCMap::ensureValue and (2) to avoid looking up HashMap twice.
    DeferGC deferGC(vm);
    return m_crossOriginFunctionMap.ensureValue(key, [&] {
        return JSFunction::create(vm, lexicalGlobalObject, length, propertyName.publicName(), nativeFunction, ImplementationVisibility::Public);
    });
}

GetterSetter* JSDOMGlobalObject::createCrossOriginGetterSetter(JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, GetValueFunc getter, PutValueFunc setter)
{
    ASSERT(getter || setter);
    auto& vm = lexicalGlobalObject->vm();
    CrossOriginMapKey key = std::make_pair(lexicalGlobalObject, getter ? getter.taggedPtr() : setter.taggedPtr());

    // WeakGCMap::ensureValue's functor must not invoke GC since GC can modify WeakGCMap in the middle of HashMap::ensure.
    // We use DeferGC here (1) not to invoke GC when executing WeakGCMap::ensureValue and (2) to avoid looking up HashMap twice.
    DeferGC deferGC(vm);
    return m_crossOriginGetterSetterMap.ensureValue(key, [&] {
        return GetterSetter::create(vm, lexicalGlobalObject,
            getter ? JSCustomGetterFunction::create(vm, lexicalGlobalObject, propertyName, getter) : nullptr,
            setter ? JSCustomSetterFunction::create(vm, lexicalGlobalObject, propertyName, setter) : nullptr);
    });
}

#if ENABLE(WEBASSEMBLY)
// https://webassembly.github.io/spec/web-api/index.html#compile-a-potential-webassembly-response
static JSC::JSPromise* handleResponseOnStreamingAction(JSC::JSGlobalObject* globalObject, JSC::JSValue source, JSC::Wasm::CompilerMode compilerMode, JSC::JSObject* importObject)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);

    auto deferred = DeferredPromise::create(*jsCast<JSDOMGlobalObject*>(globalObject), DeferredPromise::Mode::RetainPromiseOnResolve);

    auto inputResponse = JSFetchResponse::toWrapped(vm, source);
    if (!inputResponse) {
        deferred->reject(ExceptionCode::TypeError, "first argument must be an Response or Promise for Response"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    if (auto exception = inputResponse->loadingException()) {
        deferred->reject(*exception);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 4. If response is not CORS-same-origin, reject returnValue with a TypeError and abort these substeps.
    // If response is opaque, content-type becomes "".
    if (!inputResponse->isCORSSameOrigin()) {
        deferred->reject(ExceptionCode::TypeError, "Response is not CORS-same-origin"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 3. If mimeType is not `application/wasm`, reject returnValue with a TypeError and abort these substeps.
    if (!inputResponse->hasWasmMIMEType()) {
        deferred->reject(ExceptionCode::TypeError, "Unexpected response MIME type. Expected 'application/wasm'"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 5. If response’s status is not an ok status, reject returnValue with a TypeError and abort these substeps.
    if (!inputResponse->ok()) {
        deferred->reject(ExceptionCode::TypeError, "Response has not returned OK status"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // https://fetch.spec.whatwg.org/#concept-body-consume-body
    if (inputResponse->isDisturbedOrLocked()) {
        deferred->reject(ExceptionCode::TypeError, "Response is disturbed or locked"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // FIXME: for efficiency, we should load blobs directly instead of going through the readableStream path.
    if (inputResponse->isBlobBody() || inputResponse->isBlobFormData()) {
        auto streamOrException = inputResponse->readableStream(*globalObject);
        if (streamOrException.hasException()) [[unlikely]] {
            deferred->reject(streamOrException.releaseException());
            return jsCast<JSC::JSPromise*>(deferred->promise());
        }
    }

    auto compiler = JSC::Wasm::StreamingCompiler::create(vm, compilerMode, globalObject, jsCast<JSC::JSPromise*>(deferred->promise()), importObject, JSC::makeSource("handleResponseOnStreamingAction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));

    if (inputResponse->isBodyReceivedByChunk()) {
        inputResponse->consumeBodyReceivedByChunk([globalObject, compiler = WTFMove(compiler)](auto&& result) mutable {
            VM& vm = globalObject->vm();
            JSLockHolder lock(vm);

            if (result.hasException()) {
                auto exception = result.exception();
                if (exception.code() == ExceptionCode::ExistingExceptionError) {
                    auto scope = DECLARE_CATCH_SCOPE(vm);

                    EXCEPTION_ASSERT(scope.exception());

                    auto error = scope.exception()->value();
                    scope.clearException();

                    compiler->fail(globalObject, error);
                    return;
                }

                auto scope = DECLARE_THROW_SCOPE(vm);
                auto error = createDOMException(*globalObject, WTFMove(exception));
                if (scope.exception()) [[unlikely]] {
                    ASSERT(vm.hasPendingTerminationException());
                    compiler->cancel();
                    return;
                }

                compiler->fail(globalObject, error);
                return;
            }

            if (auto* chunk = result.returnValue())
                compiler->addBytes(*chunk);
            else
                compiler->finalize(globalObject);
        });
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    auto body = inputResponse->consumeBody();
    WTF::switchOn(body, [&](Ref<FormData>&) {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](Ref<SharedBuffer>& buffer) {
        compiler->addBytes(buffer->span());
        compiler->finalize(globalObject);
    }, [&](std::nullptr_t&) {
        compiler->finalize(globalObject);
    });

    return jsCast<JSC::JSPromise*>(deferred->promise());
}

JSC::JSPromise* JSDOMGlobalObject::compileStreaming(JSC::JSGlobalObject* globalObject, JSC::JSValue source)
{
    ASSERT(source);
    return handleResponseOnStreamingAction(globalObject, source, JSC::Wasm::CompilerMode::Validation, nullptr);
}

JSC::JSPromise* JSDOMGlobalObject::instantiateStreaming(JSC::JSGlobalObject* globalObject, JSC::JSValue source, JSC::JSObject* importObject)
{
    ASSERT(source);
    return handleResponseOnStreamingAction(globalObject, source, JSC::Wasm::CompilerMode::FullCompile, importObject);
}
#endif

static ScriptModuleLoader* scriptModuleLoader(JSDOMGlobalObject* globalObject)
{
    if (globalObject->inherits<JSDOMWindowBase>()) {
        if (auto document = jsCast<const JSDOMWindowBase*>(globalObject)->wrapped().documentIfLocal())
            return &document->moduleLoader();
        return nullptr;
    }
    if (globalObject->inherits<JSShadowRealmGlobalScopeBase>())
        return &jsCast<const JSShadowRealmGlobalScopeBase*>(globalObject)->wrapped().moduleLoader();
    if (globalObject->inherits<JSWorkerGlobalScopeBase>())
        return &jsCast<const JSWorkerGlobalScopeBase*>(globalObject)->wrapped().moduleLoader();
    if (globalObject->inherits<JSWorkletGlobalScopeBase>())
        return &jsCast<const JSWorkletGlobalScopeBase*>(globalObject)->wrapped().moduleLoader();
    if (globalObject->inherits<JSIDBSerializationGlobalObject>())
        return nullptr;

    dataLog("Unexpected global object: ", JSValue(globalObject), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

JSC::Identifier JSDOMGlobalObject::moduleLoaderResolve(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->resolve(globalObject, moduleLoader, moduleName, importerModuleKey, scriptFetcher);
    return { };
}

JSC::JSInternalPromise* JSDOMGlobalObject::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        RELEASE_AND_RETURN(scope, loader->fetch(globalObject, moduleLoader, moduleKey, parameters, scriptFetcher));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSValue JSDOMGlobalObject::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->evaluate(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher, awaitedValue, resumeMode);
    return JSC::jsUndefined();
}

JSC::JSInternalPromise* JSDOMGlobalObject::moduleLoaderImportModule(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSString* moduleName, JSC::JSValue parameters, const JSC::SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        RELEASE_AND_RETURN(scope, loader->importModule(globalObject, moduleLoader, moduleName, parameters, sourceOrigin));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSObject* JSDOMGlobalObject::moduleLoaderCreateImportMetaProperties(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSModuleRecord* moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->createImportMetaProperties(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
}

JSC::JSGlobalObject* JSDOMGlobalObject::deriveShadowRealmGlobalObject(JSC::JSGlobalObject* globalObject)
{
    auto& vm = globalObject->vm();

    auto domGlobalObject = jsCast<JSDOMGlobalObject*>(globalObject);
    auto context = domGlobalObject->scriptExecutionContext();
    if (auto* document = dynamicDowncast<Document>(context)) {
        // Same-origin iframes present a difficult circumstance because the
        // shadow realm global object cannot retain the incubating realm's
        // global object (that would be a refcount loop); but, same-origin
        // iframes can create objects that outlive their global object.
        //
        // Our solution is to walk up the parent tree of documents as far as
        // possible while still staying in the same origin to insure we don't
        // allow the ShadowRealm to fetch modules masquerading as the wrong
        // origin while avoiding any lifetime issues (since the topmost document
        // with a given wrapper world should outlive other objects in that
        // world)
        const auto& originalOrigin = document->securityOrigin();
        auto& originalWorld = domGlobalObject->world();

        while (!document->isTopDocument()) {
            auto* candidateDocument = document->parentDocument();
            if (!candidateDocument || !candidateDocument->protectedSecurityOrigin()->isSameOriginDomain(originalOrigin))
                break;

            document = candidateDocument;
            domGlobalObject = candidateDocument->frame()->script().globalObject(originalWorld);
        }
    }

    ASSERT(domGlobalObject);
    auto scope = ShadowRealmGlobalScope::create(domGlobalObject, scriptModuleLoader(domGlobalObject));

    auto structure = JSShadowRealmGlobalScope::createStructure(vm, nullptr, JSC::jsNull());
    auto proxyStructure = JSGlobalProxy::createStructure(vm, nullptr, JSC::jsNull());
    auto proxy = JSGlobalProxy::create(vm, proxyStructure);
    auto wrapper = JSShadowRealmGlobalScope::create(vm, structure, WTFMove(scope), proxy);

    wrapper->setPrototypeDirect(vm, wrapper->objectPrototype());
    proxy->setTarget(vm, wrapper);

    wrapper->setConsoleClient(domGlobalObject->consoleClient());

    return wrapper;
}

String JSDOMGlobalObject::defaultAgentClusterID()
{
    return makeString(Process::identifier().toUInt64(), "-default"_s);
}

String JSDOMGlobalObject::agentClusterID() const
{
    // Service workers may run in process but they need to be in a separate agent cluster.
    if (is<ServiceWorkerGlobalScope>(scriptExecutionContext()))
        return makeString(Process::identifier().toUInt64(), "-serviceworker"_s);
    if (is<SharedWorkerGlobalScope>(scriptExecutionContext()))
        return makeString(Process::identifier().toUInt64(), "-sharedworker"_s);
    return defaultAgentClusterID();
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext& context, DOMWrapperWorld& world)
{
    if (auto* document = dynamicDowncast<Document>(context))
        return toJSDOMWindow(document->frame(), world);

    if (auto* globalScope = dynamicDowncast<WorkerOrWorkletGlobalScope>(context))
        return globalScope->script()->globalScopeWrapper();

    ASSERT_NOT_REACHED();
    return nullptr;
}

static JSDOMGlobalObject& callerGlobalObject(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame* callFrame, bool skipFirstFrame, bool lookUpFromVMEntryScope)
{
    VM& vm = lexicalGlobalObject.vm();
    if (callFrame) {
        class GetCallerGlobalObjectFunctor {
        public:
            GetCallerGlobalObjectFunctor(bool skipFirstFrame)
                : m_skipFirstFrame(skipFirstFrame)
            { }

            IterationStatus operator()(StackVisitor& visitor) const
            {
                if (m_skipFirstFrame) {
                    if (!m_hasSkippedFirstFrame) {
                        m_hasSkippedFirstFrame = true;
                        return IterationStatus::Continue;
                    }
                }

                if (auto* codeBlock = visitor->codeBlock())
                    m_globalObject = codeBlock->globalObject();
                else {
                    ASSERT(visitor->callee().rawPtr());
                    // FIXME: Callee is not an object if the caller is Web Assembly.
                    // Figure out what to do here. We can probably get the global object
                    // from the top-most Wasm Instance. https://bugs.webkit.org/show_bug.cgi?id=165721
                    if (visitor->callee().isCell() && visitor->callee().asCell()->isObject())
                        m_globalObject = jsCast<JSObject*>(visitor->callee().asCell())->globalObject();
                }
                return IterationStatus::Done;
            }

            JSC::JSGlobalObject* globalObject() const { return m_globalObject; }

        private:
            bool m_skipFirstFrame { false };
            mutable bool m_hasSkippedFirstFrame { false };
            mutable JSC::JSGlobalObject* m_globalObject { nullptr };
        };

        GetCallerGlobalObjectFunctor iter(skipFirstFrame);
        StackVisitor::visit(callFrame, vm, iter);
        if (iter.globalObject())
            return *jsCast<JSDOMGlobalObject*>(iter.globalObject());
    }

    // In the case of legacyActiveGlobalObjectForAccessor, it is possible that vm.topCallFrame is nullptr when the script is evaluated as JSONP.
    // Since we put JSGlobalObject to VMEntryScope, we can retrieve the right globalObject from that.
    // For callerGlobalObject, we do not check vm.entryScope to keep it the old behavior.
    if (lookUpFromVMEntryScope) {
        if (vm.entryScope) {
            if (auto* result = vm.entryScope->globalObject())
                return *jsCast<JSDOMGlobalObject*>(result);
        }
    }

    // If we cannot find JSGlobalObject in caller frames, we just return the current lexicalGlobalObject.
    return *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
}

JSDOMGlobalObject& callerGlobalObject(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame* callFrame)
{
    constexpr bool skipFirstFrame = true;
    constexpr bool lookUpFromVMEntryScope = false;
    return callerGlobalObject(lexicalGlobalObject, callFrame, skipFirstFrame, lookUpFromVMEntryScope);
}

JSDOMGlobalObject& legacyActiveGlobalObjectForAccessor(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame* callFrame)
{
    constexpr bool skipFirstFrame = false;
    constexpr bool lookUpFromVMEntryScope = true;
    return callerGlobalObject(lexicalGlobalObject, callFrame, skipFirstFrame, lookUpFromVMEntryScope);
}

} // namespace WebCore
