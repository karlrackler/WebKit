/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include "RenderTreeUpdaterGeneratedContent.h"

#include "ContainerNodeInlines.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffectStack.h"
#include "PseudoElement.h"
#include "RenderCounter.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderQuote.h"
#include "RenderStyleInlines.h"
#include "RenderTextFragment.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "StyleTreeResolver.h"
#include "WritingSuggestionData.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeUpdater::GeneratedContent);

RenderTreeUpdater::GeneratedContent::GeneratedContent(RenderTreeUpdater& updater)
    : m_updater(updater)
{
}

void RenderTreeUpdater::GeneratedContent::updateRemainingQuotes()
{
    if (!m_updater.renderView().hasQuotesNeedingUpdate())
        return;
    updateQuotesUpTo(nullptr);
    m_previousUpdatedQuote = nullptr;
    m_updater.renderView().setHasQuotesNeedingUpdate(false);
}

void RenderTreeUpdater::GeneratedContent::updateQuotesUpTo(RenderQuote* lastQuote)
{
    auto quoteRenderers = descendantsOfType<RenderQuote>(m_updater.renderView());
    auto it = m_previousUpdatedQuote ? ++quoteRenderers.at(*m_previousUpdatedQuote) : quoteRenderers.begin();
    auto end = quoteRenderers.end();
    for (; it != end; ++it) {
        auto& quote = *it;
        // Quote character depends on quote depth so we chain the updates.
        quote.updateRenderer(m_updater.m_builder, m_previousUpdatedQuote.get());
        m_previousUpdatedQuote = quote;
        if (&quote == lastQuote)
            return;
    }
    ASSERT(!lastQuote || m_updater.m_builder.hasBrokenContinuation());
}

void RenderTreeUpdater::GeneratedContent::updateCounters()
{
    auto update = [&] {
        auto counters = m_updater.renderView().takeCountersNeedingUpdate();
        for (auto& counter : counters)
            counter.updateCounter();
    };
    // Update twice and hope it stabilizes.
    update();
    update();
}

static KeyframeEffectStack* keyframeEffectStackForElementAndPseudoId(const Element& element, PseudoId pseudoId)
{
    if (!element.mayHaveKeyframeEffects())
        return nullptr;

    return element.keyframeEffectStack(pseudoId == PseudoId::None ? std::nullopt : std::optional(Style::PseudoElementIdentifier { pseudoId }));
}

static bool needsPseudoElementForAnimation(const Element& element, PseudoId pseudoId)
{
    auto* stack = keyframeEffectStackForElementAndPseudoId(element, pseudoId);
    if (!stack)
        return false;

    return stack->requiresPseudoElement() || stack->containsProperty(CSSPropertyDisplay);
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Text& value, const String& altText, Document& document, const RenderStyle&)
{
    if (value.text.isEmpty() && altText.isEmpty())
        return { };

    auto contentRenderer = createRenderer<RenderTextFragment>(document, value.text);
    contentRenderer->setAltText(altText);
    return contentRenderer;
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Image& value, const String& altText, Document& document, const RenderStyle& pseudoStyle)
{
    auto contentRenderer = createRenderer<RenderImage>(RenderObject::Type::Image, document, RenderStyle::createStyleInheritingFromPseudoStyle(pseudoStyle), value.image.value.ptr());
    contentRenderer->initializeStyle();
    contentRenderer->setAltText(altText);
    return contentRenderer;
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Counter& value, const String&, Document& document, const RenderStyle&)
{
    return createRenderer<RenderCounter>(document, value);
}

static RenderPtr<RenderObject> createContentRenderer(const Style::Content::Quote& value, const String&, Document& document, const RenderStyle& pseudoStyle)
{
    auto contentRenderer = createRenderer<RenderQuote>(document, RenderStyle::createStyleInheritingFromPseudoStyle(pseudoStyle), value.quote);
    contentRenderer->initializeStyle();
    return contentRenderer;
}

static void createContentRenderers(RenderTreeBuilder& builder, RenderElement& pseudoRenderer, const RenderStyle& style, PseudoId pseudoId)
{
    if (auto* contentData = style.content().tryData()) {
        auto altText = contentData->altText.value_or(String { });
        for (auto& contentItem : contentData->list) {
            WTF::switchOn(contentItem,
                [&](const auto& item) {
                    if (auto child = createContentRenderer(item, altText, pseudoRenderer.document(), style); child && pseudoRenderer.isChildAllowed(*child, style))
                        builder.attach(pseudoRenderer, WTFMove(child));
                }
            );
        }
    }
#if ASSERT_ENABLED
    else {
        auto elementIsTargetedByKeyframeEffectRequiringPseudoElement = [](const Element& element, PseudoId pseudoId) {
            if (auto* stack = keyframeEffectStackForElementAndPseudoId(element, pseudoId))
                return stack->requiresPseudoElement();

            return false;
        };

        // The only valid scenario where this method is called without the "content" property being set
        // is the case where a pseudo-element has animations set on it via the Web Animations API.
        if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(pseudoRenderer.element())) {
            RefPtr hostElement = pseudoElement->hostElement();
            ASSERT(!is<PseudoElement>(hostElement));
            ASSERT(elementIsTargetedByKeyframeEffectRequiringPseudoElement(*hostElement, pseudoId));
        }
    }
#else
    UNUSED_PARAM(pseudoId);
#endif
}

static void updateStyleForContentRenderers(RenderElement& pseudoRenderer, const RenderStyle& style)
{
    for (auto& contentRenderer : descendantsOfType<RenderElement>(pseudoRenderer)) {
        // We only manage the style for the generated content which must be images or text.
        if (!is<RenderImage>(contentRenderer) && !is<RenderQuote>(contentRenderer))
            continue;
        contentRenderer.setStyle(RenderStyle::createStyleInheritingFromPseudoStyle(style));
    }
}

void RenderTreeUpdater::GeneratedContent::updateBeforeOrAfterPseudoElement(Element& current, const Style::ElementUpdate& elementUpdate, PseudoId pseudoId)
{
    ASSERT(pseudoId == PseudoId::Before || pseudoId == PseudoId::After);

    PseudoElement* pseudoElement = pseudoId == PseudoId::Before ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        m_updater.renderTreePosition().invalidateNextSibling(*renderer);

    auto* updateStyle = (elementUpdate.style && elementUpdate.style->hasCachedPseudoStyles()) ? elementUpdate.style->getCachedPseudoStyle({ pseudoId }) : nullptr;

    ASSERT(!is<PseudoElement>(current));
    if (!needsPseudoElement(updateStyle) && !needsPseudoElementForAnimation(current, pseudoId)) {
        if (pseudoElement) {
            if (pseudoId == PseudoId::Before)
                removeBeforePseudoElement(current, m_updater.m_builder);
            else
                removeAfterPseudoElement(current, m_updater.m_builder);
        }
        return;
    }

    if (!updateStyle)
        return;

    auto* existingStyle = pseudoElement ? pseudoElement->renderOrDisplayContentsStyle() : nullptr;

    auto styleChanges = existingStyle ? Style::determineChanges(*updateStyle, *existingStyle) : Style::Change::Renderer;
    if (!styleChanges)
        return;

    pseudoElement = &current.ensurePseudoElement(pseudoId);

    if (updateStyle->display() == DisplayType::Contents) {
        // For display:contents we create an inline wrapper that inherits its
        // style from the display:contents style.
        auto contentsStyle = RenderStyle::createPtr();
        contentsStyle->setPseudoElementType(pseudoId);
        contentsStyle->inheritFrom(*updateStyle);
        contentsStyle->copyContentFrom(*updateStyle);
        contentsStyle->copyPseudoElementsFrom(*updateStyle);

        Style::ElementUpdate contentsUpdate { WTFMove(contentsStyle), styleChanges, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTFMove(contentsUpdate));
        auto pseudoElementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*updateStyle);
        pseudoElement->storeDisplayContentsOrNoneStyle(makeUnique<RenderStyle>(WTFMove(pseudoElementUpdateStyle)));
    } else {
        auto pseudoElementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*updateStyle);
        Style::ElementUpdate pseudoElementUpdate { makeUnique<RenderStyle>(WTFMove(pseudoElementUpdateStyle)), styleChanges, elementUpdate.recompositeLayer };
        m_updater.updateElementRenderer(*pseudoElement, WTFMove(pseudoElementUpdate));
        if (updateStyle->display() == DisplayType::None) {
            auto pseudoElementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*updateStyle);
            pseudoElement->storeDisplayContentsOrNoneStyle(makeUnique<RenderStyle>(WTFMove(pseudoElementUpdateStyle)));
        } else
            pseudoElement->clearDisplayContentsOrNoneStyle();
    }

    auto* pseudoElementRenderer = pseudoElement->renderer();
    if (!pseudoElementRenderer)
        return;

    if (styleChanges.contains(Style::Change::Renderer))
        createContentRenderers(m_updater.m_builder, *pseudoElementRenderer, *updateStyle, pseudoId);
    else
        updateStyleForContentRenderers(*pseudoElementRenderer, *updateStyle);

    if (m_updater.renderView().hasQuotesNeedingUpdate()) {
        for (auto& child : descendantsOfType<RenderQuote>(*pseudoElementRenderer))
            updateQuotesUpTo(&child);
    }
    m_updater.m_builder.updateAfterDescendants(*pseudoElementRenderer);
}

void RenderTreeUpdater::GeneratedContent::updateBackdropRenderer(RenderElement& renderer, StyleDifference minimalStyleDifference)
{
    auto destroyBackdropIfNeeded = [&renderer, this]() {
        if (WeakPtr backdropRenderer = renderer.backdropRenderer())
            m_updater.m_builder.destroy(*backdropRenderer);
    };

    // Intentionally bail out early here to avoid computing the style.
    if (!renderer.element() || !renderer.element()->isInTopLayer()) {
        destroyBackdropIfNeeded();
        return;
    }

    auto style = renderer.getCachedPseudoStyle({ PseudoId::Backdrop }, &renderer.style());
    if (!style || style->display() == DisplayType::None) {
        destroyBackdropIfNeeded();
        return;
    }

    auto newStyle = RenderStyle::clone(*style);
    if (auto backdropRenderer = renderer.backdropRenderer())
        backdropRenderer->setStyle(WTFMove(newStyle), minimalStyleDifference);
    else {
        auto newBackdropRenderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, renderer.document(), WTFMove(newStyle));
        newBackdropRenderer->initializeStyle();
        renderer.setBackdropRenderer(*newBackdropRenderer.get());
        m_updater.m_builder.attach(renderer.view(), WTFMove(newBackdropRenderer));
    }
}

bool RenderTreeUpdater::GeneratedContent::needsPseudoElement(const RenderStyle* style)
{
    if (!style)
        return false;
    if (!m_updater.renderTreePosition().parent().canHaveGeneratedChildren())
        return false;
    if (!pseudoElementRendererIsNeeded(style))
        return false;
    return true;
}

void RenderTreeUpdater::GeneratedContent::removeBeforePseudoElement(Element& element, RenderTreeBuilder& builder)
{
    auto* pseudoElement = element.beforePseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearBeforePseudoElement();
}

void RenderTreeUpdater::GeneratedContent::removeAfterPseudoElement(Element& element, RenderTreeBuilder& builder)
{
    auto* pseudoElement = element.afterPseudoElement();
    if (!pseudoElement)
        return;
    tearDownRenderers(*pseudoElement, TeardownType::Full, builder);
    element.clearAfterPseudoElement();
}

void RenderTreeUpdater::GeneratedContent::updateWritingSuggestionsRenderer(RenderElement& renderer, StyleDifference minimalStyleDifference)
{
    auto destroyWritingSuggestionsIfNeeded = [&renderer, this]() {
        if (!renderer.element())
            return;

        auto& editor = renderer.element()->document().editor();

        if (WeakPtr writingSuggestionsRenderer = editor.writingSuggestionRenderer())
            m_updater.m_builder.destroy(*writingSuggestionsRenderer);
    };

    if (!renderer.canHaveChildren())
        return;

    if (!renderer.element())
        return;

    auto& editor = renderer.element()->document().editor();
    RefPtr nodeBeforeWritingSuggestions = editor.nodeBeforeWritingSuggestions();
    if (!nodeBeforeWritingSuggestions)
        return;

    if (renderer.element() != nodeBeforeWritingSuggestions->parentElement())
        return;

    auto* writingSuggestionData = editor.writingSuggestionData();
    if (!writingSuggestionData) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    auto style = renderer.getCachedPseudoStyle({ PseudoId::InternalWritingSuggestions }, &renderer.style());
    if (!style || style->display() == DisplayType::None) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    WeakPtr nodeBeforeWritingSuggestionsTextRenderer = dynamicDowncast<RenderText>(nodeBeforeWritingSuggestions->renderer());
    if (!nodeBeforeWritingSuggestionsTextRenderer) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    WeakPtr parentForWritingSuggestions = nodeBeforeWritingSuggestionsTextRenderer->parent();
    if (!parentForWritingSuggestions) {
        destroyWritingSuggestionsIfNeeded();
        return;
    }

    auto textWithoutSuggestion = nodeBeforeWritingSuggestionsTextRenderer->text();

    auto [prefix, suffix] = [&] -> std::pair<String, String> {
        if (!writingSuggestionData->supportsSuffix())
            return { textWithoutSuggestion, emptyString() };

        auto offset = writingSuggestionData->offset();
        return { textWithoutSuggestion.substring(0, offset), textWithoutSuggestion.substring(offset) };
    }();

    nodeBeforeWritingSuggestionsTextRenderer->setText(prefix);

    auto newStyle = RenderStyle::clone(*style);
    newStyle.setDisplay(DisplayType::Inline);

    if (auto writingSuggestionsRenderer = editor.writingSuggestionRenderer()) {
        writingSuggestionsRenderer->setStyle(WTFMove(newStyle), minimalStyleDifference);

        auto* writingSuggestionsText = dynamicDowncast<RenderText>(writingSuggestionsRenderer->firstChild());
        if (!writingSuggestionsText) {
            ASSERT_NOT_REACHED();
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        writingSuggestionsText->setText(writingSuggestionData->content());

        if (!suffix.isEmpty()) {
            auto* suffixText = dynamicDowncast<RenderText>(writingSuggestionsRenderer->nextSibling());
            if (!suffixText) {
                ASSERT_NOT_REACHED();
                destroyWritingSuggestionsIfNeeded();
                return;
            }

            suffixText->setText(suffix);
        }
    } else {
        auto newWritingSuggestionsRenderer = WebCore::createRenderer<RenderInline>(RenderObject::Type::Inline, renderer.document(), WTFMove(newStyle));
        newWritingSuggestionsRenderer->initializeStyle();

        WeakPtr rendererAfterWritingSuggestions = nodeBeforeWritingSuggestionsTextRenderer->nextSibling();

        auto writingSuggestionsText = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, renderer.document(), writingSuggestionData->content());
        m_updater.m_builder.attach(*newWritingSuggestionsRenderer, WTFMove(writingSuggestionsText));

        editor.setWritingSuggestionRenderer(*newWritingSuggestionsRenderer.get());
        m_updater.m_builder.attach(*parentForWritingSuggestions, WTFMove(newWritingSuggestionsRenderer), rendererAfterWritingSuggestions.get());

        if (!parentForWritingSuggestions) {
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        auto* prefixNode = nodeBeforeWritingSuggestionsTextRenderer->textNode();
        if (!prefixNode) {
            ASSERT_NOT_REACHED();
            destroyWritingSuggestionsIfNeeded();
            return;
        }

        if (!suffix.isEmpty()) {
            auto suffixRenderer = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, *prefixNode, suffix);
            m_updater.m_builder.attach(*parentForWritingSuggestions, WTFMove(suffixRenderer), rendererAfterWritingSuggestions.get());
        }
    }
}

}
