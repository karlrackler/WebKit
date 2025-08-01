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
#include "JSTestAsyncKeyValueIterable.h"

#include "ActiveDOMObject.h"
#include "ContextDestructionObserverInlines.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSDOMAsyncIterator.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructorNotConstructable.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMOperation.h"
#include "JSDOMWrapperCache.h"
#include "JSTestNode.h"
#include "ScriptExecutionContext.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

// Functions

static JSC_DECLARE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_entries);
static JSC_DECLARE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_keys);
static JSC_DECLARE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_values);

// Attributes

static JSC_DECLARE_CUSTOM_GETTER(jsTestAsyncKeyValueIterableConstructor);

class JSTestAsyncKeyValueIterablePrototype final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSTestAsyncKeyValueIterablePrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestAsyncKeyValueIterablePrototype* ptr = new (NotNull, JSC::allocateCell<JSTestAsyncKeyValueIterablePrototype>(vm)) JSTestAsyncKeyValueIterablePrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestAsyncKeyValueIterablePrototype, Base);
        return &vm.plainObjectSpace();
    }
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestAsyncKeyValueIterablePrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSTestAsyncKeyValueIterablePrototype, JSTestAsyncKeyValueIterablePrototype::Base);

using JSTestAsyncKeyValueIterableDOMConstructor = JSDOMConstructorNotConstructable<JSTestAsyncKeyValueIterable>;

template<> const ClassInfo JSTestAsyncKeyValueIterableDOMConstructor::s_info = { "TestAsyncKeyValueIterable"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestAsyncKeyValueIterableDOMConstructor) };

template<> JSValue JSTestAsyncKeyValueIterableDOMConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(vm);
    return globalObject.functionPrototype();
}

template<> void JSTestAsyncKeyValueIterableDOMConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    JSString* nameString = jsNontrivialString(vm, "TestAsyncKeyValueIterable"_s);
    m_originalName.set(vm, this, nameString);
    putDirect(vm, vm.propertyNames->name, nameString, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->prototype, JSTestAsyncKeyValueIterable::prototype(vm, globalObject), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::DontDelete);
}

/* Hash table for prototype */

static const std::array<HashTableValue, 4> JSTestAsyncKeyValueIterablePrototypeTableValues {
    HashTableValue { "constructor"_s, static_cast<unsigned>(PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsTestAsyncKeyValueIterableConstructor, 0 } },
    HashTableValue { "entries"_s, static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { HashTableValue::NativeFunctionType, jsTestAsyncKeyValueIterablePrototypeFunction_entries, 0 } },
    HashTableValue { "keys"_s, static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { HashTableValue::NativeFunctionType, jsTestAsyncKeyValueIterablePrototypeFunction_keys, 0 } },
    HashTableValue { "values"_s, static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { HashTableValue::NativeFunctionType, jsTestAsyncKeyValueIterablePrototypeFunction_values, 0 } },
};

const ClassInfo JSTestAsyncKeyValueIterablePrototype::s_info = { "TestAsyncKeyValueIterable"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestAsyncKeyValueIterablePrototype) };

void JSTestAsyncKeyValueIterablePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestAsyncKeyValueIterable::info(), JSTestAsyncKeyValueIterablePrototypeTableValues, *this);
    putDirect(vm, vm.propertyNames->asyncIteratorSymbol, getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

const ClassInfo JSTestAsyncKeyValueIterable::s_info = { "TestAsyncKeyValueIterable"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestAsyncKeyValueIterable) };

JSTestAsyncKeyValueIterable::JSTestAsyncKeyValueIterable(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestAsyncKeyValueIterable>&& impl)
    : JSDOMWrapper<TestAsyncKeyValueIterable>(structure, globalObject, WTFMove(impl))
{
}

static_assert(!std::is_base_of<ActiveDOMObject, TestAsyncKeyValueIterable>::value, "Interface is not marked as [ActiveDOMObject] even though implementation class subclasses ActiveDOMObject.");

JSObject* JSTestAsyncKeyValueIterable::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    auto* structure = JSTestAsyncKeyValueIterablePrototype::createStructure(vm, &globalObject, globalObject.objectPrototype());
    structure->setMayBePrototype(true);
    return JSTestAsyncKeyValueIterablePrototype::create(vm, &globalObject, structure);
}

JSObject* JSTestAsyncKeyValueIterable::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSTestAsyncKeyValueIterable>(vm, globalObject);
}

JSValue JSTestAsyncKeyValueIterable::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestAsyncKeyValueIterableDOMConstructor, DOMConstructorID::TestAsyncKeyValueIterable>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

void JSTestAsyncKeyValueIterable::destroy(JSC::JSCell* cell)
{
    JSTestAsyncKeyValueIterable* thisObject = static_cast<JSTestAsyncKeyValueIterable*>(cell);
    thisObject->JSTestAsyncKeyValueIterable::~JSTestAsyncKeyValueIterable();
}

JSC_DEFINE_CUSTOM_GETTER(jsTestAsyncKeyValueIterableConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSTestAsyncKeyValueIterablePrototype*>(JSValue::decode(thisValue));
    if (!prototype) [[unlikely]]
        return throwVMTypeError(lexicalGlobalObject, throwScope);
    return JSValue::encode(JSTestAsyncKeyValueIterable::getConstructor(vm, prototype->globalObject()));
}

struct TestAsyncKeyValueIterableIteratorTraits {
    static constexpr JSDOMIteratorType type = JSDOMIteratorType::Map;
    using KeyType = IDLUSVString;
    using ValueType = IDLInterface<TestNode>;
};

using TestAsyncKeyValueIterableIteratorBase = JSDOMAsyncIteratorBase<JSTestAsyncKeyValueIterable, TestAsyncKeyValueIterableIteratorTraits>;
class TestAsyncKeyValueIterableIterator final : public TestAsyncKeyValueIterableIteratorBase {
public:
    using Base = TestAsyncKeyValueIterableIteratorBase;
    DECLARE_INFO;

    template<typename, SubspaceAccess mode> static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        if constexpr (mode == JSC::SubspaceAccess::Concurrently)
            return nullptr;
        return WebCore::subspaceForImpl<TestAsyncKeyValueIterableIterator, UseCustomHeapCellType::No>(vm, "TestAsyncKeyValueIterableIterator"_s,
            [] (auto& spaces) { return spaces.m_clientSubspaceForTestAsyncKeyValueIterableIterator.get(); },
            [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForTestAsyncKeyValueIterableIterator = std::forward<decltype(space)>(space); },
            [] (auto& spaces) { return spaces.m_subspaceForTestAsyncKeyValueIterableIterator.get(); },
            [] (auto& spaces, auto&& space) { spaces.m_subspaceForTestAsyncKeyValueIterableIterator = std::forward<decltype(space)>(space); }
        );
    }

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static TestAsyncKeyValueIterableIterator* create(JSC::VM& vm, JSC::Structure* structure, JSTestAsyncKeyValueIterable& iteratedObject, IterationKind kind)
    {
        auto* instance = new (NotNull, JSC::allocateCell<TestAsyncKeyValueIterableIterator>(vm)) TestAsyncKeyValueIterableIterator(structure, iteratedObject, kind);
        instance->finishCreation(vm);
        return instance;
    }

    JSC::JSBoundFunction* createOnSettledFunction(JSC::JSGlobalObject*);
    JSC::JSBoundFunction* createOnFulfilledFunction(JSC::JSGlobalObject*);
    JSC::JSBoundFunction* createOnRejectedFunction(JSC::JSGlobalObject*);
private:
    TestAsyncKeyValueIterableIterator(JSC::Structure* structure, JSTestAsyncKeyValueIterable& iteratedObject, IterationKind kind)
        : Base(structure, iteratedObject, kind)
    {
    }
};

using TestAsyncKeyValueIterableIteratorPrototype = JSDOMAsyncIteratorPrototype<JSTestAsyncKeyValueIterable, TestAsyncKeyValueIterableIteratorTraits>;
JSC_ANNOTATE_HOST_FUNCTION(TestAsyncKeyValueIterableIteratorPrototypeNext, TestAsyncKeyValueIterableIteratorPrototype::next);

template<>
const JSC::ClassInfo TestAsyncKeyValueIterableIteratorBase::s_info = { "TestAsyncKeyValueIterableBase Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestAsyncKeyValueIterableIteratorBase) };
const JSC::ClassInfo TestAsyncKeyValueIterableIterator::s_info = { "TestAsyncKeyValueIterable Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestAsyncKeyValueIterableIterator) };

template<>
const JSC::ClassInfo TestAsyncKeyValueIterableIteratorPrototype::s_info = { "TestAsyncKeyValueIterable Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestAsyncKeyValueIterableIteratorPrototype) };

static inline EncodedJSValue jsTestAsyncKeyValueIterablePrototypeFunction_entriesCaller(JSGlobalObject*, CallFrame*, JSTestAsyncKeyValueIterable* thisObject)
{
    return JSValue::encode(iteratorCreate<TestAsyncKeyValueIterableIterator>(*thisObject, IterationKind::Entries));
}

JSC_DEFINE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_entries, (JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSTestAsyncKeyValueIterable>::call<jsTestAsyncKeyValueIterablePrototypeFunction_entriesCaller>(*lexicalGlobalObject, *callFrame, "entries");
}

static inline EncodedJSValue jsTestAsyncKeyValueIterablePrototypeFunction_keysCaller(JSGlobalObject*, CallFrame*, JSTestAsyncKeyValueIterable* thisObject)
{
    return JSValue::encode(iteratorCreate<TestAsyncKeyValueIterableIterator>(*thisObject, IterationKind::Keys));
}

JSC_DEFINE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_keys, (JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSTestAsyncKeyValueIterable>::call<jsTestAsyncKeyValueIterablePrototypeFunction_keysCaller>(*lexicalGlobalObject, *callFrame, "keys");
}

static inline EncodedJSValue jsTestAsyncKeyValueIterablePrototypeFunction_valuesCaller(JSGlobalObject*, CallFrame*, JSTestAsyncKeyValueIterable* thisObject)
{
    return JSValue::encode(iteratorCreate<TestAsyncKeyValueIterableIterator>(*thisObject, IterationKind::Values));
}

JSC_DEFINE_HOST_FUNCTION(jsTestAsyncKeyValueIterablePrototypeFunction_values, (JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSTestAsyncKeyValueIterable>::call<jsTestAsyncKeyValueIterablePrototypeFunction_valuesCaller>(*lexicalGlobalObject, *callFrame, "values");
}

JSC_ANNOTATE_HOST_FUNCTION(TestAsyncKeyValueIterableIteratorBaseOnPromiseSettled, TestAsyncKeyValueIterableIteratorBase::onPromiseSettled);
JSC_ANNOTATE_HOST_FUNCTION(TestAsyncKeyValueIterableIteratorBaseOnPromiseFulfilled, TestAsyncKeyValueIterableIteratorBase::onPromiseFulFilled);
JSC_ANNOTATE_HOST_FUNCTION(TestAsyncKeyValueIterableIteratorBaseOnPromiseRejected, TestAsyncKeyValueIterableIteratorBase::onPromiseRejected);
JSC::GCClient::IsoSubspace* JSTestAsyncKeyValueIterable::subspaceForImpl(JSC::VM& vm)
{
    return WebCore::subspaceForImpl<JSTestAsyncKeyValueIterable, UseCustomHeapCellType::No>(vm, "JSTestAsyncKeyValueIterable"_s,
        [] (auto& spaces) { return spaces.m_clientSubspaceForTestAsyncKeyValueIterable.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForTestAsyncKeyValueIterable = std::forward<decltype(space)>(space); },
        [] (auto& spaces) { return spaces.m_subspaceForTestAsyncKeyValueIterable.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForTestAsyncKeyValueIterable = std::forward<decltype(space)>(space); }
    );
}

void JSTestAsyncKeyValueIterable::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSTestAsyncKeyValueIterable*>(cell);
    analyzer.setWrappedObjectForCell(cell, &thisObject->wrapped());
    if (RefPtr context = thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, makeString("url "_s, context->url().string()));
    Base::analyzeHeap(cell, analyzer);
}

bool JSTestAsyncKeyValueIterableOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    UNUSED_PARAM(reason);
    return false;
}

void JSTestAsyncKeyValueIterableOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestAsyncKeyValueIterable = static_cast<JSTestAsyncKeyValueIterable*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, jsTestAsyncKeyValueIterable->protectedWrapped().ptr(), jsTestAsyncKeyValueIterable);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestAsyncKeyValueIterable@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore25TestAsyncKeyValueIterableE[]; }
#endif
template<std::same_as<TestAsyncKeyValueIterable> T>
static inline void verifyVTable(TestAsyncKeyValueIterable* ptr) 
{
    if constexpr (std::is_polymorphic_v<T>) {
        const void* actualVTablePointer = getVTablePointer<T>(ptr);
#if PLATFORM(WIN)
        void* expectedVTablePointer = __identifier("??_7TestAsyncKeyValueIterable@WebCore@@6B@");
#else
        void* expectedVTablePointer = &_ZTVN7WebCore25TestAsyncKeyValueIterableE[2];
#endif

        // If you hit this assertion you either have a use after free bug, or
        // TestAsyncKeyValueIterable has subclasses. If TestAsyncKeyValueIterable has subclasses that get passed
        // to toJS() we currently require TestAsyncKeyValueIterable you to opt out of binding hardening
        // by adding the SkipVTableValidation attribute to the interface IDL definition
        RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
    }
}
#endif
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<TestAsyncKeyValueIterable>&& impl)
{
#if ENABLE(BINDING_INTEGRITY)
    verifyVTable<TestAsyncKeyValueIterable>(impl.ptr());
#endif
    return createWrapper<TestAsyncKeyValueIterable>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, TestAsyncKeyValueIterable& impl)
{
    return wrap(lexicalGlobalObject, globalObject, impl);
}

TestAsyncKeyValueIterable* JSTestAsyncKeyValueIterable::toWrapped(JSC::VM&, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestAsyncKeyValueIterable*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
