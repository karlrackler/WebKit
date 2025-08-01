/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelectorList.h"
#include "MediaQuery.h"
#include "RuleData.h"
#include "RuleFeature.h"
#include "SelectorCompiler.h"
#include "StyleRule.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/VectorHash.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSelector;
class StyleSheetContents;
class StyleRulePositionTry;
class StyleRuleViewTransition;

namespace MQ {
class MediaQueryEvaluator;
}

namespace Style {

class Resolver;
class RuleSet;

using CascadeLayerPriority = uint16_t;

struct RuleSetAndNegation {
    RefPtr<const RuleSet> ruleSet;
    IsNegation isNegation { IsNegation::No };
};
using InvalidationRuleSetVector = Vector<RuleSetAndNegation, 1>;

struct DynamicMediaQueryEvaluationChanges {
    enum class Type { InvalidateStyle, ResetStyle };
    Type type;
    InvalidationRuleSetVector invalidationRuleSets { };

    void append(DynamicMediaQueryEvaluationChanges&& other)
    {
        type = std::max(type, other.type);
        if (type == Type::ResetStyle)
            invalidationRuleSets.clear();
        else
            invalidationRuleSets.appendVector(WTFMove(other.invalidationRuleSets));
    };
};

class RuleSet : public RefCounted<RuleSet> {
    WTF_MAKE_NONCOPYABLE(RuleSet);
public:
    static Ref<RuleSet> create() { return adoptRef(*new RuleSet); }

    ~RuleSet();

    typedef Vector<RuleData, 1> RuleDataVector;
    typedef HashMap<AtomString, std::unique_ptr<RuleDataVector>> AtomRuleMap;

    void addRule(const StyleRule&, unsigned selectorIndex, unsigned selectorListIndex);
    void addPageRule(StyleRulePage&);
    void setViewTransitionRule(StyleRuleViewTransition&);
    RefPtr<StyleRuleViewTransition> viewTransitionRule() const;

    void addToRuleSet(const AtomString& key, AtomRuleMap&, const RuleData&);
    void shrinkToFit();

    bool hasViewportDependentMediaQueries() const { return m_hasViewportDependentMediaQueries; }

    std::optional<DynamicMediaQueryEvaluationChanges> evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator&);

    const RuleFeatureSet& features() const { return m_features; }

    const RuleDataVector* idRules(const AtomString& key) const { return m_idRules.get(key); }
    const RuleDataVector* classRules(const AtomString& key) const { return m_classRules.get(key); }
    const RuleDataVector* attributeRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* tagRules(const AtomString& key, bool isHTMLName) const;
    const RuleDataVector* userAgentPartRules(const AtomString& key) const { return m_userAgentPartRules.get(key); }
    const RuleDataVector* linkPseudoClassRules() const { return &m_linkPseudoClassRules; }
    const RuleDataVector* namedPseudoElementRules(const AtomString& key) const { return m_namedPseudoElementRules.get(key); }
#if ENABLE(VIDEO)
    const RuleDataVector& cuePseudoRules() const { return m_cuePseudoRules; }
#endif
    const RuleDataVector& hostPseudoClassRules() const { return m_hostPseudoClassRules; }
    const RuleDataVector& slottedPseudoElementRules() const { return m_slottedPseudoElementRules; }
    const RuleDataVector& partPseudoElementRules() const { return m_partPseudoElementRules; }
    const RuleDataVector* focusPseudoClassRules() const { return &m_focusPseudoClassRules; }
    const RuleDataVector* rootElementRules() const { return &m_rootElementRules; }
    const RuleDataVector* universalRules() const { return &m_universalRules; }

    const Vector<StyleRulePage*>& pageRules() const { return m_pageRules; }

    unsigned ruleCount() const { return m_ruleCount; }

    bool hasAttributeRules() const { return !m_attributeLocalNameRules.isEmpty(); }
    bool hasUserAgentPartRules() const { return !m_userAgentPartRules.isEmpty(); }
    bool hasHostPseudoClassRulesMatchingInShadowTree() const { return m_hasHostPseudoClassRulesMatchingInShadowTree; }
    bool hasHostOrScopePseudoClassRulesInUniversalBucket() const { return m_hasHostOrScopePseudoClassRulesInUniversalBucket; }

    static constexpr auto cascadeLayerPriorityForPresentationalHints = std::numeric_limits<CascadeLayerPriority>::min();
    static constexpr auto cascadeLayerPriorityForUnlayered = std::numeric_limits<CascadeLayerPriority>::max();

    CascadeLayerPriority cascadeLayerPriorityFor(const RuleData&) const;

    bool hasContainerQueries() const { return !m_containerQueries.isEmpty(); }
    Vector<const CQ::ContainerQuery*> containerQueriesFor(const RuleData&) const;
    Vector<Ref<const StyleRuleContainer>> containerQueryRules() const;

    bool hasScopeRules() const { return !m_scopeRules.isEmpty(); }
    Vector<Ref<const StyleRuleScope>> scopeRulesFor(const RuleData&) const;

    const RefPtr<const StyleRulePositionTry> positionTryRuleForName(const AtomString&) const;

private:
    friend class RuleSetBuilder;

    RuleSet();

    using CascadeLayerIdentifier = unsigned;
    using ContainerQueryIdentifier = unsigned;
    using ScopeRuleIdentifier = unsigned;

    void addRule(RuleData&&, CascadeLayerIdentifier, ContainerQueryIdentifier, ScopeRuleIdentifier);

    struct ResolverMutatingRule {
        Ref<StyleRuleBase> rule;
        CascadeLayerIdentifier layerIdentifier;
    };

    struct CollectedMediaQueryChanges {
        bool requiredFullReset { false };
        Vector<size_t> changedQueryIndexes { };
        Vector<Vector<Ref<const StyleRule>>*> affectedRules { };
    };
    CollectedMediaQueryChanges evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator&, size_t startIndex);

    template<typename Function> void traverseRuleDatas(Function&&);

    struct CascadeLayer {
        CascadeLayerName resolvedName;
        CascadeLayerIdentifier parentIdentifier;
        CascadeLayerPriority priority { 0 };
    };
    CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) { return m_cascadeLayers[identifier - 1]; }
    const CascadeLayer& cascadeLayerForIdentifier(CascadeLayerIdentifier identifier) const { return m_cascadeLayers[identifier - 1]; }
    CascadeLayerPriority cascadeLayerPriorityForIdentifier(CascadeLayerIdentifier) const;

    struct ScopeAndParent {
        Ref<const StyleRuleScope> scopeRule;
        ScopeRuleIdentifier parent;
    };

    struct ContainerQueryAndParent {
        Ref<const StyleRuleContainer> containerRule;
        ContainerQueryIdentifier parent;
    };

    struct DynamicMediaQueryRules {
        Vector<MQ::MediaQueryList> mediaQueries;
        Vector<size_t> affectedRulePositions;
        Vector<Ref<const StyleRule>> affectedRules;
        bool requiresFullReset { false };
        bool result { true };

        void shrinkToFit()
        {
            mediaQueries.shrinkToFit();
            affectedRulePositions.shrinkToFit();
            affectedRules.shrinkToFit();
        }
    };

    AtomRuleMap m_idRules;
    AtomRuleMap m_classRules;
    AtomRuleMap m_attributeLocalNameRules;
    AtomRuleMap m_attributeLowercaseLocalNameRules;
    AtomRuleMap m_tagLocalNameRules;
    AtomRuleMap m_tagLowercaseLocalNameRules;
    AtomRuleMap m_userAgentPartRules;
    AtomRuleMap m_namedPseudoElementRules;
    RuleDataVector m_linkPseudoClassRules;
#if ENABLE(VIDEO)
    RuleDataVector m_cuePseudoRules;
#endif
    RuleDataVector m_hostPseudoClassRules;
    RuleDataVector m_slottedPseudoElementRules;
    RuleDataVector m_partPseudoElementRules;
    RuleDataVector m_focusPseudoClassRules;
    RuleDataVector m_rootElementRules;
    RuleDataVector m_universalRules;
    Vector<StyleRulePage*> m_pageRules;
    RefPtr<StyleRuleViewTransition> m_viewTransitionRule;
    RuleFeatureSet m_features;
    Vector<DynamicMediaQueryRules> m_dynamicMediaQueryRules;
    HashMap<Vector<size_t>, Ref<const RuleSet>> m_mediaQueryInvalidationRuleSetCache;
    unsigned m_ruleCount { 0 };

    Vector<CascadeLayer> m_cascadeLayers;
    // This is a side vector to hold layer identifiers without bloating RuleData.
    Vector<CascadeLayerIdentifier> m_cascadeLayerIdentifierForRulePosition;

    Vector<ResolverMutatingRule> m_resolverMutatingRulesInLayers;

    Vector<ContainerQueryAndParent> m_containerQueries;
    Vector<ContainerQueryIdentifier> m_containerQueryIdentifierForRulePosition;

    // @scope
    Vector<ScopeAndParent> m_scopeRules;
    Vector<ScopeRuleIdentifier> m_scopeRuleIdentifierForRulePosition;

    // @position-try
    HashMap<AtomString, RefPtr<const StyleRulePositionTry>> m_positionTryRules;

    bool m_hasHostPseudoClassRulesMatchingInShadowTree { false };
    bool m_hasViewportDependentMediaQueries { false };
    bool m_hasHostOrScopePseudoClassRulesInUniversalBucket { false };
};

inline const RuleSet::RuleDataVector* RuleSet::attributeRules(const AtomString& key, bool isHTMLName) const
{
    auto& rules = isHTMLName ? m_attributeLowercaseLocalNameRules : m_attributeLocalNameRules;
    return rules.get(key);
}

inline const RuleSet::RuleDataVector* RuleSet::tagRules(const AtomString& key, bool isHTMLName) const
{
    auto& rules = isHTMLName ? m_tagLowercaseLocalNameRules : m_tagLocalNameRules;
    return rules.get(key);
}

inline CascadeLayerPriority RuleSet::cascadeLayerPriorityForIdentifier(CascadeLayerIdentifier identifier) const
{
    if (!identifier)
        return cascadeLayerPriorityForUnlayered;
    return cascadeLayerForIdentifier(identifier).priority;
}

inline CascadeLayerPriority RuleSet::cascadeLayerPriorityFor(const RuleData& ruleData) const
{
    if (m_cascadeLayerIdentifierForRulePosition.size() <= ruleData.position())
        return cascadeLayerPriorityForUnlayered;
    auto identifier = m_cascadeLayerIdentifierForRulePosition[ruleData.position()];
    return cascadeLayerPriorityForIdentifier(identifier);
}

inline Vector<const CQ::ContainerQuery*> RuleSet::containerQueriesFor(const RuleData& ruleData) const
{
    if (m_containerQueryIdentifierForRulePosition.size() <= ruleData.position())
        return { };

    Vector<const CQ::ContainerQuery*> queries;

    auto identifier = m_containerQueryIdentifierForRulePosition[ruleData.position()];
    while (identifier) {
        auto& query = m_containerQueries[identifier - 1];
        queries.append(&query.containerRule->containerQuery());
        identifier = query.parent;
    };

    return queries;
}

} // namespace Style
} // namespace WebCore
