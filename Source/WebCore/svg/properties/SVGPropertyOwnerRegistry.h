/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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

#pragma once

#include "SVGAnimatedPropertyAccessorImpl.h"
#include "SVGAnimatedPropertyPairAccessorImpl.h"
#include "SVGPropertyAccessorImpl.h"
#include "SVGPropertyRegistry.h"
#include <wtf/HashMap.h>

namespace WebCore {

class SVGRectElement;
class SVGCircleElement;

class SVGAttributeAnimator;
class SVGConditionalProcessingAttributes;

template<typename T>
concept HasFastPropertyForAttribute = requires(const T& element, const QualifiedName& name)
{
    { element.propertyForAttribute(name) } -> std::same_as<SVGAnimatedProperty*>;
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom().impl(), key.localName().impl(), key.namespaceURI().impl() };
            return computeHash(components);
        }
        return DefaultHash<QualifiedName>::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }

    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    static constexpr bool hasHashInValue = true;
};

template<typename OwnerType, typename... BaseTypes>
class SVGPropertyOwnerRegistry : public SVGPropertyRegistry {
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(SVGPropertyOwnerRegistry);
public:
    SVGPropertyOwnerRegistry(OwnerType& owner)
        : m_owner(owner)
    {
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGStringList> SVGConditionalProcessingAttributes::*property>
    static void registerConditionalProcessingAttributeProperty()
    {
        registerProperty(attributeName, SVGConditionalProcessingAttributeAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGTransformList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGTransformListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedBoolean> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedBooleanAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, typename EnumType, Ref<SVGAnimatedEnumeration> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedEnumerationAccessor<OwnerType, EnumType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedInteger> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedIntegerAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedLength> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedLengthAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedLengthList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedLengthListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedNumber> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedNumberAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedNumberList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedNumberListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedAngle> OwnerType::*property1, Ref<SVGAnimatedOrientType> OwnerType::*property2>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedAngleOrientAccessor<OwnerType>::template singleton<property1, property2>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedPathSegList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPathSegListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedPointList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPointListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedPreserveAspectRatio> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedPreserveAspectRatioAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedRect> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedRectAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedString> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedStringAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedTransformList> OwnerType::*property>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedTransformListAccessor<OwnerType>::template singleton<property>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedInteger> OwnerType::*property1, Ref<SVGAnimatedInteger> OwnerType::*property2>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedIntegerPairAccessor<OwnerType>::template singleton<property1, property2>());
    }

    template<const LazyNeverDestroyed<const QualifiedName>& attributeName, Ref<SVGAnimatedNumber> OwnerType::*property1, Ref<SVGAnimatedNumber> OwnerType::*property2>
    static void registerProperty()
    {
        registerProperty(attributeName, SVGAnimatedNumberPairAccessor<OwnerType>::template singleton<property1, property2>());
    }

    // Enumerate all the SVGMemberAccessors recursively. The functor will be called and will
    // be given the pair<QualifiedName, SVGMemberAccessor> till the functor returns false.
    template<typename Functor>
    static bool enumerateRecursively(NOESCAPE const Functor& functor)
    {
        for (const auto& entry : attributeNameToAccessorMap()) {
            if (!functor(entry))
                return false;
        }
        return enumerateRecursivelyBaseTypes(functor);
    }

    template<typename Functor>
    static bool lookupRecursivelyAndApply(const QualifiedName& attributeName, NOESCAPE const Functor& functor)
    {
        if (auto* accessor = findAccessor(attributeName)) {
            functor(*accessor);
            return true;
        }
        return lookupRecursivelyAndApplyBaseTypes(attributeName, functor);
    }

    // Returns true if OwnerType owns a property whose name is attributeName.
    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return findAccessor(attributeName);
    }

    // Returns true if OwnerType owns a property whose name is attributeName
    // and its type is SVGAnimatedLength.
    static bool isAnimatedLengthAttribute(const QualifiedName& attributeName)
    {
        if (const auto* accessor = findAccessor(attributeName))
            return accessor->isAnimatedLength();
        return false;
    }

    QualifiedName propertyAttributeName(const SVGProperty& property) const override
    {
        QualifiedName attributeName = nullQName();
        enumerateRecursively([&](const auto& entry) -> bool {
            if (!entry.value->matches(m_owner, property))
                return true;
            attributeName = entry.key;
            return false;
        });
        return attributeName;
    }

    QualifiedName animatedPropertyAttributeName(const SVGAnimatedProperty& animatedProperty) const override
    {
        QualifiedName attributeName = nullQName();
        enumerateRecursively([&](const auto& entry) -> bool {
            if (!entry.value->matches(m_owner, animatedProperty))
                return true;
            attributeName = entry.key;
            return false;
        });
        return attributeName;
    }

    void setAnimatedPropertyDirty(const QualifiedName& attributeName, SVGAnimatedProperty& animatedProperty) const override
    {
        if (auto* property = fastAnimatedPropertyLookup(m_owner, attributeName)) {
            property->setDirty();
            return;
        }

        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            accessor.setDirty(m_owner, animatedProperty);
        });
    }

    // Detach all the properties recursively from their OwnerTypes.
    void detachAllProperties() const override
    {
        enumerateRecursively([&](const auto& entry) -> bool {
            entry.value->detach(m_owner);
            return true;
        });
    }

    static inline SVGAnimatedProperty* fastAnimatedPropertyLookup(OwnerType& owner, const QualifiedName& attributeName)
    {
        if constexpr (HasFastPropertyForAttribute<OwnerType>)
            return owner.propertyForAttribute(attributeName);
        else {
            static_assert(!std::is_same_v<OwnerType, SVGRectElement> && !std::is_same_v<OwnerType, SVGCircleElement>, "Element should use fast property path");
            return nullptr;
        }
    }

    // Finds the property whose name is attributeName and returns the synchronize
    // string through the associated SVGMemberAccessor.
    std::optional<String> synchronize(const QualifiedName& attributeName) const override
    {
        if (auto* property = fastAnimatedPropertyLookup(m_owner, attributeName))
            return property->synchronize();

        std::optional<String> value;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            value = accessor.synchronize(m_owner);
        });
        return value;
    }

    // Enumerate recursively the SVGMemberAccessors of the OwnerType and all its BaseTypes.
    // Collect all the pairs <AttributeName, String> only for the dirty properties.
    HashMap<QualifiedName, String> synchronizeAllAttributes() const override
    {
        HashMap<QualifiedName, String> map;
        enumerateRecursively([&](const auto& entry) -> bool {
            if (auto string = entry.value->synchronize(m_owner))
                map.add(entry.key, *string);
            return true;
        });
        return map;
    }

    bool isAnimatedPropertyAttribute(const QualifiedName& attributeName) const override
    {
        if (auto* property = fastAnimatedPropertyLookup(m_owner, attributeName))
            return true;

        bool isAnimatedPropertyAttribute = false;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            isAnimatedPropertyAttribute = accessor.isAnimatedProperty();
        });
        return isAnimatedPropertyAttribute;
    }

    bool isAnimatedStylePropertyAttribute(const QualifiedName& attributeName) const override
    {
        static NeverDestroyed<HashSet<QualifiedName::QualifiedNameImpl*>> animatedStyleAttributes = std::initializer_list<QualifiedName::QualifiedNameImpl*> {
            SVGNames::cxAttr->impl(),
            SVGNames::cyAttr->impl(),
            SVGNames::rAttr->impl(),
            SVGNames::rxAttr->impl(),
            SVGNames::ryAttr->impl(),
            SVGNames::heightAttr->impl(),
            SVGNames::widthAttr->impl(),
            SVGNames::xAttr->impl(),
            SVGNames::yAttr->impl()
        };
        return isAnimatedLengthAttribute(attributeName) && animatedStyleAttributes.get().contains(attributeName.impl());
    }

    RefPtr<SVGAttributeAnimator> createAnimator(const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const override
    {
        RefPtr<SVGAttributeAnimator> animator;
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            animator = accessor.createAnimator(m_owner, attributeName, animationMode, calcMode, isAccumulated, isAdditive);
        });
        return animator;
    }

    void appendAnimatedInstance(const QualifiedName& attributeName, SVGAttributeAnimator& animator) const override
    {
        lookupRecursivelyAndApply(attributeName, [&](auto& accessor) {
            accessor.appendAnimatedInstance(m_owner, animator);
        });
    }

private:
    // Singleton map for every OwnerType.
    using QualifiedNameAccessorHashMap = HashMap<QualifiedName, const SVGMemberAccessor<OwnerType>*, SVGAttributeHashTranslator>;

    static QualifiedNameAccessorHashMap& attributeNameToAccessorMap()
    {
        static NeverDestroyed<QualifiedNameAccessorHashMap> attributeNameToAccessorMap;
        return attributeNameToAccessorMap;
    }

    static void registerProperty(const QualifiedName& attributeName, const SVGMemberAccessor<OwnerType>& propertyAccessor)
    {
        attributeNameToAccessorMap().add(attributeName, &propertyAccessor);
    }

    template<typename Functor, size_t I = 0>
    static bool enumerateRecursivelyBaseTypes(NOESCAPE const Functor&)
        requires (I == sizeof...(BaseTypes))
    {
        return true;
    }

    template<typename Functor, size_t I = 0>
    static bool enumerateRecursivelyBaseTypes(NOESCAPE const Functor& functor)
        requires (I < sizeof...(BaseTypes))
    {
        using BaseType = std::tuple_element_t<I, std::tuple<BaseTypes...>>;

        if (!BaseType::PropertyRegistry::enumerateRecursively(functor))
            return false;

        return enumerateRecursivelyBaseTypes<Functor, I + 1>(functor);
    }

    static const SVGMemberAccessor<OwnerType>* findAccessor(const QualifiedName& attributeName)
    {
        auto it = attributeNameToAccessorMap().find(attributeName);
        return it != attributeNameToAccessorMap().end() ? it->value : nullptr;
    }

    template<typename Functor, size_t I = 0>
    static bool lookupRecursivelyAndApplyBaseTypes(const QualifiedName&, NOESCAPE const Functor&)
        requires (I == sizeof...(BaseTypes))
    {
        return false;
    }

    template<typename Functor, size_t I = 0>
    static bool lookupRecursivelyAndApplyBaseTypes(const QualifiedName& attributeName, NOESCAPE const Functor& functor)
        requires (I < sizeof...(BaseTypes))
    {
        using BaseType = std::tuple_element_t<I, std::tuple<BaseTypes...>>;

        if (BaseType::PropertyRegistry::lookupRecursivelyAndApply(attributeName, functor))
            return true;

        return lookupRecursivelyAndApplyBaseTypes<Functor, I + 1>(attributeName, functor);
    }

    OwnerType& m_owner;
};

#define TZONE_TEMPLATE_PARAMS template<typename OwnerType, typename... BaseTypes>
#define TZONE_TYPE SVGPropertyOwnerRegistry<OwnerType, BaseTypes...>

WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS();

#undef TZONE_TEMPLATE_PARAMS
#undef TZONE_TYPE

} // namespace WebCore
