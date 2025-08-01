/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSExposedToWorkerAndWindow.h"

#include "ActiveDOMObject.h"
#include "ContextDestructionObserverInlines.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMOperation.h"
#include "JSDOMWrapperCache.h"
#include "JSTestObj.h"
#include "ScriptExecutionContext.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

template<> ConversionResult<IDLDictionary<ExposedToWorkerAndWindow::Dict>> convertDictionary<ExposedToWorkerAndWindow::Dict>(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    bool isNullOrUndefined = value.isUndefinedOrNull();
    auto* object = isNullOrUndefined ? nullptr : value.getObject();
    if (!isNullOrUndefined && !object) [[unlikely]] {
        throwTypeError(&lexicalGlobalObject, throwScope);
        return ConversionResultException { };
    }
    ExposedToWorkerAndWindow::Dict result;
    JSValue objValue;
    if (isNullOrUndefined)
        objValue = jsUndefined();
    else {
        objValue = object->get(&lexicalGlobalObject, Identifier::fromString(vm, "obj"_s));
        RETURN_IF_EXCEPTION(throwScope, ConversionResultException { });
    }
    if (!objValue.isUndefined()) {
        auto objConversionResult = convert<IDLInterface<TestObj>>(lexicalGlobalObject, objValue);
        if (objConversionResult.hasException(throwScope)) [[unlikely]]
            return ConversionResultException { };
        result.obj = objConversionResult.releaseReturnValue();
    }
    return result;
}

JSC::JSObject* convertDictionaryToJS(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, const ExposedToWorkerAndWindow::Dict& dictionary)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto result = constructEmptyObject(&lexicalGlobalObject, globalObject.objectPrototype());

    if (!IDLInterface<TestObj>::isNullValue(dictionary.obj)) {
        auto objValue = toJS<IDLInterface<TestObj>>(lexicalGlobalObject, globalObject, throwScope, IDLInterface<TestObj>::extractValueFromNullable(dictionary.obj));
        RETURN_IF_EXCEPTION(throwScope, { });
        result->putDirect(vm, JSC::Identifier::fromString(vm, "obj"_s), objValue);
    }
    return result;
}

// Functions

static JSC_DECLARE_HOST_FUNCTION(jsExposedToWorkerAndWindowPrototypeFunction_doSomething);

// Attributes

static JSC_DECLARE_CUSTOM_GETTER(jsExposedToWorkerAndWindowConstructor);

class JSExposedToWorkerAndWindowPrototype final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSExposedToWorkerAndWindowPrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSExposedToWorkerAndWindowPrototype* ptr = new (NotNull, JSC::allocateCell<JSExposedToWorkerAndWindowPrototype>(vm)) JSExposedToWorkerAndWindowPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSExposedToWorkerAndWindowPrototype, Base);
        return &vm.plainObjectSpace();
    }
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSExposedToWorkerAndWindowPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSExposedToWorkerAndWindowPrototype, JSExposedToWorkerAndWindowPrototype::Base);

using JSExposedToWorkerAndWindowDOMConstructor = JSDOMConstructor<JSExposedToWorkerAndWindow>;

template<> EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSExposedToWorkerAndWindowDOMConstructor::construct(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* castedThis = jsCast<JSExposedToWorkerAndWindowDOMConstructor*>(callFrame->jsCallee());
    ASSERT(castedThis);
    auto object = ExposedToWorkerAndWindow::create();
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    static_assert(TypeOrExceptionOrUnderlyingType<decltype(object)>::isRef);
    auto jsValue = toJSNewlyCreated<IDLInterface<ExposedToWorkerAndWindow>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WTFMove(object));
    if constexpr (IsExceptionOr<decltype(object)>)
        RETURN_IF_EXCEPTION(throwScope, { });
    setSubclassStructureIfNeeded<ExposedToWorkerAndWindow>(lexicalGlobalObject, callFrame, asObject(jsValue));
    RETURN_IF_EXCEPTION(throwScope, { });
    return JSValue::encode(jsValue);
}
JSC_ANNOTATE_HOST_FUNCTION(JSExposedToWorkerAndWindowDOMConstructorConstruct, JSExposedToWorkerAndWindowDOMConstructor::construct);

template<> const ClassInfo JSExposedToWorkerAndWindowDOMConstructor::s_info = { "ExposedToWorkerAndWindow"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSExposedToWorkerAndWindowDOMConstructor) };

template<> JSValue JSExposedToWorkerAndWindowDOMConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSExposedToWorkerAndWindowDOMConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    JSString* nameString = jsNontrivialString(vm, "ExposedToWorkerAndWindow"_s);
    m_originalName.set(vm, this, nameString);
    putDirect(vm, vm.propertyNames->name, nameString, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->prototype, JSExposedToWorkerAndWindow::prototype(vm, globalObject), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::DontDelete);
}

/* Hash table for prototype */

static const std::array<HashTableValue, 2> JSExposedToWorkerAndWindowPrototypeTableValues {
    HashTableValue { "constructor"_s, static_cast<unsigned>(PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsExposedToWorkerAndWindowConstructor, 0 } },
    HashTableValue { "doSomething"_s, static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { HashTableValue::NativeFunctionType, jsExposedToWorkerAndWindowPrototypeFunction_doSomething, 0 } },
};

const ClassInfo JSExposedToWorkerAndWindowPrototype::s_info = { "ExposedToWorkerAndWindow"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSExposedToWorkerAndWindowPrototype) };

void JSExposedToWorkerAndWindowPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSExposedToWorkerAndWindow::info(), JSExposedToWorkerAndWindowPrototypeTableValues, *this);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

const ClassInfo JSExposedToWorkerAndWindow::s_info = { "ExposedToWorkerAndWindow"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSExposedToWorkerAndWindow) };

JSExposedToWorkerAndWindow::JSExposedToWorkerAndWindow(Structure* structure, JSDOMGlobalObject& globalObject, Ref<ExposedToWorkerAndWindow>&& impl)
    : JSDOMWrapper<ExposedToWorkerAndWindow>(structure, globalObject, WTFMove(impl))
{
}

static_assert(!std::is_base_of<ActiveDOMObject, ExposedToWorkerAndWindow>::value, "Interface is not marked as [ActiveDOMObject] even though implementation class subclasses ActiveDOMObject.");

JSObject* JSExposedToWorkerAndWindow::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    auto* structure = JSExposedToWorkerAndWindowPrototype::createStructure(vm, &globalObject, globalObject.objectPrototype());
    structure->setMayBePrototype(true);
    return JSExposedToWorkerAndWindowPrototype::create(vm, &globalObject, structure);
}

JSObject* JSExposedToWorkerAndWindow::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSExposedToWorkerAndWindow>(vm, globalObject);
}

JSValue JSExposedToWorkerAndWindow::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSExposedToWorkerAndWindowDOMConstructor, DOMConstructorID::ExposedToWorkerAndWindow>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

void JSExposedToWorkerAndWindow::destroy(JSC::JSCell* cell)
{
    JSExposedToWorkerAndWindow* thisObject = static_cast<JSExposedToWorkerAndWindow*>(cell);
    thisObject->JSExposedToWorkerAndWindow::~JSExposedToWorkerAndWindow();
}

JSC_DEFINE_CUSTOM_GETTER(jsExposedToWorkerAndWindowConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSExposedToWorkerAndWindowPrototype*>(JSValue::decode(thisValue));
    if (!prototype) [[unlikely]]
        return throwVMTypeError(lexicalGlobalObject, throwScope);
    return JSValue::encode(JSExposedToWorkerAndWindow::getConstructor(vm, prototype->globalObject()));
}

static inline JSC::EncodedJSValue jsExposedToWorkerAndWindowPrototypeFunction_doSomethingBody(JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame, typename IDLOperation<JSExposedToWorkerAndWindow>::ClassParameter castedThis)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(callFrame);
    SUPPRESS_UNCOUNTED_LOCAL auto& impl = castedThis->wrapped();
    RELEASE_AND_RETURN(throwScope, JSValue::encode(toJS<IDLDictionary<ExposedToWorkerAndWindow::Dict>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, impl.doSomething())));
}

JSC_DEFINE_HOST_FUNCTION(jsExposedToWorkerAndWindowPrototypeFunction_doSomething, (JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame))
{
    return IDLOperation<JSExposedToWorkerAndWindow>::call<jsExposedToWorkerAndWindowPrototypeFunction_doSomethingBody>(*lexicalGlobalObject, *callFrame, "doSomething");
}

JSC::GCClient::IsoSubspace* JSExposedToWorkerAndWindow::subspaceForImpl(JSC::VM& vm)
{
    return WebCore::subspaceForImpl<JSExposedToWorkerAndWindow, UseCustomHeapCellType::No>(vm, "JSExposedToWorkerAndWindow"_s,
        [] (auto& spaces) { return spaces.m_clientSubspaceForExposedToWorkerAndWindow.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForExposedToWorkerAndWindow = std::forward<decltype(space)>(space); },
        [] (auto& spaces) { return spaces.m_subspaceForExposedToWorkerAndWindow.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForExposedToWorkerAndWindow = std::forward<decltype(space)>(space); }
    );
}

void JSExposedToWorkerAndWindow::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSExposedToWorkerAndWindow*>(cell);
    analyzer.setWrappedObjectForCell(cell, &thisObject->wrapped());
    if (RefPtr context = thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, makeString("url "_s, context->url().string()));
    Base::analyzeHeap(cell, analyzer);
}

bool JSExposedToWorkerAndWindowOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    UNUSED_PARAM(reason);
    return false;
}

void JSExposedToWorkerAndWindowOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsExposedToWorkerAndWindow = static_cast<JSExposedToWorkerAndWindow*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsExposedToWorkerAndWindow->protectedWrapped().ptr(), jsExposedToWorkerAndWindow);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7ExposedToWorkerAndWindow@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore24ExposedToWorkerAndWindowE[]; }
#endif
template<std::same_as<ExposedToWorkerAndWindow> T>
static inline void verifyVTable(ExposedToWorkerAndWindow* ptr) 
{
    if constexpr (std::is_polymorphic_v<T>) {
        const void* actualVTablePointer = getVTablePointer<T>(ptr);
#if PLATFORM(WIN)
        void* expectedVTablePointer = __identifier("??_7ExposedToWorkerAndWindow@WebCore@@6B@");
#else
        void* expectedVTablePointer = &_ZTVN7WebCore24ExposedToWorkerAndWindowE[2];
#endif

        // If you hit this assertion you either have a use after free bug, or
        // ExposedToWorkerAndWindow has subclasses. If ExposedToWorkerAndWindow has subclasses that get passed
        // to toJS() we currently require ExposedToWorkerAndWindow you to opt out of binding hardening
        // by adding the SkipVTableValidation attribute to the interface IDL definition
        RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
    }
}
#endif
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<ExposedToWorkerAndWindow>&& impl)
{
#if ENABLE(BINDING_INTEGRITY)
    verifyVTable<ExposedToWorkerAndWindow>(impl.ptr());
#endif
    return createWrapper<ExposedToWorkerAndWindow>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, ExposedToWorkerAndWindow& impl)
{
    return wrap(lexicalGlobalObject, globalObject, impl);
}

ExposedToWorkerAndWindow* JSExposedToWorkerAndWindow::toWrapped(JSC::VM&, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSExposedToWorkerAndWindow*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
