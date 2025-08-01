/*
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "ApplyStyleCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSSerializationContext.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "EditingInlines.h"
#include "Editor.h"
#include "ElementChildIteratorInlines.h"
#include "HTMLFontElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "LocalFrame.h"
#include "NodeInlines.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "RenderLineBreak.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "ScriptDisallowedScope.h"
#include "StyleExtractor.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "Text.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

static String& styleSpanClassString()
{
    static NeverDestroyed<String> styleSpanClassString = String::fromLatin1(AppleStyleSpanClass);
    return styleSpanClassString;
}

bool isLegacyAppleStyleSpan(const Node* node)
{
    RefPtr spanElement = dynamicDowncast<HTMLSpanElement>(node);
    return spanElement && spanElement->attributeWithoutSynchronization(classAttr) == styleSpanClassString();
}

static bool hasNoAttributeOrOnlyStyleAttribute(const StyledElement& element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    if (!element.hasAttributes())
        return true;

    unsigned matchedAttributes = 0;
    if (element.attributeWithoutSynchronization(classAttr) == styleSpanClassString())
        matchedAttributes++;
    if (element.hasAttribute(styleAttr) && (shouldStyleAttributeBeEmpty == AllowNonEmptyStyleAttribute
        || !element.inlineStyle() || element.inlineStyle()->isEmpty()))
        matchedAttributes++;

    ASSERT(matchedAttributes <= element.attributeCount());
    return matchedAttributes == element.attributeCount();
}

bool isStyleSpanOrSpanWithOnlyStyleAttribute(const Element& element)
{
    RefPtr spanElement = dynamicDowncast<HTMLSpanElement>(element);
    return spanElement && hasNoAttributeOrOnlyStyleAttribute(*spanElement, AllowNonEmptyStyleAttribute);
}

static inline bool isSpanWithoutAttributesOrUnstyledStyleSpan(const Element& element)
{
    RefPtr spanElement = dynamicDowncast<HTMLSpanElement>(element);
    return spanElement && hasNoAttributeOrOnlyStyleAttribute(*spanElement, StyleAttributeShouldBeEmpty);
}

bool isEmptyFontTag(const Element* element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    RefPtr fontElement = dynamicDowncast<HTMLFontElement>(element);
    return fontElement && hasNoAttributeOrOnlyStyleAttribute(*fontElement, shouldStyleAttributeBeEmpty);
}

static Ref<HTMLElement> createFontElement(Document& document)
{
    return createHTMLElement(document, fontTag);
}

Ref<HTMLElement> createStyleSpanElement(Document& document)
{
    return createHTMLElement(document, spanTag);
}

ApplyStyleCommand::ApplyStyleCommand(Ref<Document>&& document, const EditingStyle* style, EditAction editingAction, ApplyStylePropertyLevel propertyLevel)
    : CompositeEditCommand(WTFMove(document), editingAction)
    , m_style(style->copy())
    , m_propertyLevel(propertyLevel)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Ref<Document>&& document, const EditingStyle* style, const Position& start, const Position& end, EditAction editingAction, ApplyStylePropertyLevel propertyLevel)
    : CompositeEditCommand(WTFMove(document), editingAction)
    , m_style(style->copy())
    , m_propertyLevel(propertyLevel)
    , m_start(start)
    , m_end(end)
    , m_useEndingSelection(false)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Ref<Element>&& element, bool removeOnly, EditAction editingAction)
    : CompositeEditCommand(element->document(), editingAction)
    , m_style(EditingStyle::create())
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(WTFMove(element))
    , m_removeOnly(removeOnly)
{
}

ApplyStyleCommand::ApplyStyleCommand(Ref<Document>&& document, const EditingStyle* style, IsInlineElementToRemoveFunction isInlineElementToRemoveFunction, EditAction editingAction)
    : CompositeEditCommand(WTFMove(document), editingAction)
    , m_style(style->copy())
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_removeOnly(true)
    , m_isInlineElementToRemoveFunction(isInlineElementToRemoveFunction)
{
}

void ApplyStyleCommand::updateStartEnd(const Position& newStart, const Position& newEnd)
{
    ASSERT(!is_gt(treeOrder<ShadowIncludingTree>(newStart, newEnd)));

    if (!m_useEndingSelection && (newStart != m_start || newEnd != m_end))
        m_useEndingSelection = true;

    bool wasBaseFirst = startingSelection().isBaseFirst() || startingSelection().directionality() != Directionality::Strong;
    setEndingSelection(VisibleSelection(VisiblePosition(wasBaseFirst ? newStart : newEnd), VisiblePosition(wasBaseFirst ? newEnd : newStart),
        endingSelection().directionality()));
    m_start = newStart;
    m_end = newEnd;
}

Position ApplyStyleCommand::startPosition()
{
    if (m_useEndingSelection)
        return endingSelection().start();
    
    return m_start;
}

Position ApplyStyleCommand::endPosition()
{
    if (m_useEndingSelection)
        return endingSelection().end();
    
    return m_end;
}

void ApplyStyleCommand::doApply()
{
    switch (m_propertyLevel) {
    case ApplyStylePropertyLevel::Default: {
        // Apply the block-centric properties of the style.
        auto blockStyle = m_style->extractAndRemoveBlockProperties();
        if (!blockStyle->isEmpty())
            applyBlockStyle(blockStyle);
        // Apply any remaining styles to the inline elements.
        if (!m_style->isEmpty() || m_styledInlineElement || m_isInlineElementToRemoveFunction) {
            applyRelativeFontStyleChange(m_style.get());
            applyInlineStyle(*m_style);
        }
        break;
    }
    case ApplyStylePropertyLevel::ForceBlock:
        // Force all properties to be applied as block styles.
        applyBlockStyle(*m_style);
        break;
    }
}

void ApplyStyleCommand::applyBlockStyle(EditingStyle& style)
{
    // Update document layout once before removing styles so that we avoid the expense of
    // updating before each and every call to check a computed style.
    document().updateLayoutIgnorePendingStylesheets();

    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    VisiblePosition visibleStart(start);
    VisiblePosition visibleEnd(end);

    if (visibleStart.isNull() || visibleStart.isOrphan() || visibleEnd.isNull() || visibleEnd.isOrphan())
        return;

    // Save and restore the selection endpoints using their indices in the editable root, since
    // addBlockStyleIfNeeded may moveParagraphs, which can remove these endpoints.
    // Calculate start and end indices from the start of the tree that they're in.
    RefPtr scopeRoot { highestEditableRoot(visibleStart.deepEquivalent()) };
    if (!scopeRoot)
        return;

    auto scope = makeRangeSelectingNodeContents(*scopeRoot);
    auto range = *makeSimpleRange(visibleStart, visibleEnd);
    auto startIndex = characterCount({ scope.start, range.start }, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
    auto endIndex = characterCount({ scope.start, range.end }, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);

    VisiblePosition paragraphStart(startOfParagraph(visibleStart));
    VisiblePosition nextParagraphStart(endOfParagraph(paragraphStart).next());
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        visibleEnd = visibleEnd.previous(CannotCrossEditingBoundary);
    VisiblePosition beyondEnd(endOfParagraph(visibleEnd).next());
    while (paragraphStart.isNotNull() && paragraphStart != beyondEnd) {
        StyleChange styleChange(&style, paragraphStart.deepEquivalent());
        if (styleChange.cssStyle() || m_removeOnly) {
            RefPtr<Node> block = enclosingBlock(paragraphStart.deepEquivalent().protectedDeprecatedNode());
            if (!m_removeOnly) {
                RefPtr<Node> newBlock = moveParagraphContentsToNewBlockIfNecessary(paragraphStart.deepEquivalent());
                if (newBlock)
                    block = newBlock;
            }
            ASSERT(!block || is<Element>(*block));
            if (RefPtr htmlBlock = dynamicDowncast<HTMLElement>(block.get())) {
                removeCSSStyle(style, *htmlBlock);
                if (!m_removeOnly)
                    addBlockStyle(styleChange, *htmlBlock);
            }

            if (nextParagraphStart.isOrphan())
                nextParagraphStart = endOfParagraph(paragraphStart).next();
        }

        paragraphStart = nextParagraphStart;
        nextParagraphStart = endOfParagraph(paragraphStart).next();
    }
    
    auto startPosition = makeDeprecatedLegacyPosition(resolveCharacterLocation(scope, startIndex, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions));
    auto endPosition = makeDeprecatedLegacyPosition(resolveCharacterLocation(scope, endIndex, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions));
    updateStartEnd(startPosition, endPosition);
}

static Ref<MutableStyleProperties> copyStyleOrCreateEmpty(const StyleProperties* style)
{
    if (!style)
        return MutableStyleProperties::create();
    return style->mutableCopy();
}

void ApplyStyleCommand::applyRelativeFontStyleChange(EditingStyle* style)
{
    static const float MinimumFontSize = 0.1f;

    if (!style || !style->hasFontSizeDelta())
        return;

    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    if (start.treeScope() != end.treeScope())
        return;

    // Join up any adjacent text nodes.
    if (is<Text>(start.deprecatedNode())) {
        joinChildTextNodes(start.deprecatedNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }
    
    if (start.isNull() || end.isNull())
        return;

    if (is<Text>(*end.deprecatedNode()) && start.deprecatedNode()->parentNode() != end.deprecatedNode()->parentNode()) {
        joinChildTextNodes(end.deprecatedNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // Split the start text nodes if needed to apply style.
    if (isValidCaretPositionInTextNode(start)) {
        splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    if (isValidCaretPositionInTextNode(end)) {
        splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // Calculate loop end point.
    // If the end node is before the start node (can only happen if the end node is
    // an ancestor of the start node), we gather nodes up to the next sibling of the end node.
    RefPtr<Node> beyondEnd;
    ASSERT(start.deprecatedNode());
    ASSERT(end.deprecatedNode());
    if (end.deprecatedNode()->contains(*start.deprecatedNode()))
        beyondEnd = NodeTraversal::nextSkippingChildren(*end.deprecatedNode());
    else
        beyondEnd = NodeTraversal::next(*end.deprecatedNode());
    
    start = start.upstream(); // Move upstream to ensure we do not add redundant spans.
    RefPtr startNode = start.deprecatedNode();

    if (!startNode)
        return;
    
    // Ensure the startNode is not at or past the beyondEnd when node traversal
    // is performed in the following loops below.
    if (beyondEnd) {
        auto treeOrderPos = treeOrder(*startNode, *beyondEnd);
        if (is_gteq(treeOrderPos))
            return;
    }
    
    if (is<Text>(*startNode) && start.deprecatedEditingOffset() >= caretMaxOffset(*startNode)) {
        // Move out of text node if range does not include its characters.
        startNode = NodeTraversal::next(*startNode);
        if (!startNode)
            return;
    }

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    HashMap<Ref<Node>, float> startingFontSizes;
    for (auto node = startNode; node != beyondEnd; node = NodeTraversal::next(*node)) {
        RELEASE_ASSERT(node);
        startingFontSizes.set(*node, computedFontSize(node.get()));
    }

    // These spans were added by us. If empty after font size changes, they can be removed.
    Vector<Ref<HTMLElement>> unstyledSpans;

    RefPtr<Node> lastStyledNode;
    bool reachedEnd = false;
    for (auto node = startNode; node != beyondEnd && !reachedEnd; node = NodeTraversal::next(*node)) {
        RELEASE_ASSERT(node);

        RefPtr<HTMLElement> element;
        if (auto* htmlElement = dynamicDowncast<HTMLElement>(*node)) {
            // Only work on fully selected nodes.
            if (!nodeFullySelected(*htmlElement, start, end)) {
                if (!node->isConnected())
                    break;
                continue;
            }
            element = htmlElement;
        } else if (is<Text>(*node) && node->renderer() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            auto span = createStyleSpanElement(document());
            if (!surroundNodeRangeWithElement(*node, *node, span))
                continue;
            reachedEnd = node->isDescendantOf(beyondEnd.get()) || !node->parentElement();
            element = WTFMove(span);
        }  else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;

        RefPtr<MutableStyleProperties> inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
        float currentFontSize = computedFontSize(node.get());
        float desiredFontSize = std::max(MinimumFontSize, startingFontSizes.get(node.get()) + style->fontSizeDelta());
        RefPtr<CSSValue> value = inlineStyle->getPropertyCSSValue(CSSPropertyFontSize);
        if (value) {
            element->removeInlineStyleProperty(CSSPropertyFontSize);
            currentFontSize = computedFontSize(node.get());
        }
        if (currentFontSize != desiredFontSize) {
            inlineStyle->setProperty(CSSPropertyFontSize, CSSPrimitiveValue::create(desiredFontSize, CSSUnitType::CSS_PX));
            setNodeAttribute(*element, styleAttr, inlineStyle->asTextAtom(CSS::defaultSerializationContext()));
        }
        if (inlineStyle->isEmpty()) {
            removeNodeAttribute(*element, styleAttr);
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(*element))
                unstyledSpans.append(element.releaseNonNull());
        }
    }

    for (auto& unstyledSpan : unstyledSpans)
        removeNodePreservingChildren(unstyledSpan);
}

static ContainerNode* dummySpanAncestorForNode(Node* node)
{
    RefPtr<Node> currentNode = node;
    while (currentNode) {
        RefPtr element = dynamicDowncast<Element>(*currentNode);
        if (element && isStyleSpanOrSpanWithOnlyStyleAttribute(*element))
            break;
        currentNode = currentNode->parentNode();
    }
    return currentNode ? currentNode->parentNode() : nullptr;
}

void ApplyStyleCommand::cleanupUnstyledAppleStyleSpans(ContainerNode* dummySpanAncestor)
{
    if (!dummySpanAncestor)
        return;

    // Dummy spans are created when text node is split, so that style information
    // can be propagated, which can result in more splitting. If a dummy span gets
    // cloned/split, the new node is always a sibling of it. Therefore, we scan
    // all the children of the dummy's parent

    Vector<Ref<Element>> toRemove;
    for (Ref child : childrenOfType<Element>(*dummySpanAncestor)) {
        if (isSpanWithoutAttributesOrUnstyledStyleSpan(child))
            toRemove.append(child);
    }

    for (Ref element : toRemove)
        removeNodePreservingChildren(element.get());
}

RefPtr<HTMLElement> ApplyStyleCommand::splitAncestorsWithUnicodeBidi(Node* node, bool before, WritingDirection allowedDirection)
{
    // We are allowed to leave the highest ancestor with unicode-bidi unsplit if it is unicode-bidi: embed and direction: allowedDirection.
    // In that case, we return the unsplit ancestor. Otherwise, we return 0.
    RefPtr block { enclosingBlock(node) };
    if (!block || block == node)
        return nullptr;

    RefPtr<Node> highestAncestorWithUnicodeBidi;
    RefPtr<Node> nextHighestAncestorWithUnicodeBidi;
    int highestAncestorUnicodeBidi = 0;
    for (auto ancestor = RefPtr { node->parentNode() }; ancestor != block; ancestor = ancestor->parentNode()) {
        int unicodeBidi = valueID(Style::Extractor(ancestor.get()).propertyValue(CSSPropertyUnicodeBidi).get());
        if (unicodeBidi && unicodeBidi != CSSValueNormal) {
            highestAncestorUnicodeBidi = unicodeBidi;
            nextHighestAncestorWithUnicodeBidi = highestAncestorWithUnicodeBidi;
            highestAncestorWithUnicodeBidi = ancestor;
        }
    }

    if (!highestAncestorWithUnicodeBidi)
        return nullptr;

    RefPtr<HTMLElement> unsplitAncestor;

    if (allowedDirection != WritingDirection::Natural && highestAncestorUnicodeBidi != CSSValueBidiOverride && is<HTMLElement>(*highestAncestorWithUnicodeBidi)) {
        auto highestAncestorDirection = EditingStyle::create(highestAncestorWithUnicodeBidi.get(), EditingStyle::PropertiesToInclude::AllProperties)->textDirection();
        if (highestAncestorDirection && *highestAncestorDirection == allowedDirection) {
            if (!nextHighestAncestorWithUnicodeBidi)
                return static_pointer_cast<HTMLElement>(WTFMove(highestAncestorWithUnicodeBidi));

            unsplitAncestor = static_pointer_cast<HTMLElement>(WTFMove(highestAncestorWithUnicodeBidi));
            highestAncestorWithUnicodeBidi = nextHighestAncestorWithUnicodeBidi;
        }
    }

    // Split every ancestor through highest ancestor with embedding.
    RefPtr<Node> currentNode = node;
    while (currentNode) {
        RefPtr parent = downcast<Element>(currentNode->parentNode());
        if (before ? currentNode->previousSibling() : currentNode->nextSibling())
            splitElement(*parent, before ? *currentNode : *currentNode->protectedNextSibling());
        if (parent == highestAncestorWithUnicodeBidi)
            break;
        currentNode = parent;
    }

    return unsplitAncestor;
}

void ApplyStyleCommand::removeEmbeddingUpToEnclosingBlock(Node* node, Node* unsplitAncestor)
{
    RefPtr block { enclosingBlock(node) };
    if (!block || block == node)
        return;

    for (RefPtr<Node> ancestor = node->parentNode(), parent; ancestor != block && ancestor != unsplitAncestor; ancestor = parent) {
        parent = ancestor->parentNode();
        RefPtr element = dynamicDowncast<StyledElement>(*ancestor);
        if (!element)
            continue;

        int unicodeBidi = valueID(Style::Extractor(element.get()).propertyValue(CSSPropertyUnicodeBidi).get());
        if (!unicodeBidi || unicodeBidi == CSSValueNormal)
            continue;

        // FIXME: This code should really consider the mapped attribute 'dir', the inline style declaration,
        // and all matching style rules in order to determine how to best set the unicode-bidi property to 'normal'.
        // For now, it assumes that if the 'dir' attribute is present, then removing it will suffice, and
        // otherwise it sets the property in the inline style declaration.
        if (element->hasAttributeWithoutSynchronization(dirAttr)) {
            // FIXME: If this is a BDO element, we should probably just remove it if it has no
            // other attributes, like we (should) do with B and I elements.
            removeNodeAttribute(*element, dirAttr);
        } else {
            auto inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
            inlineStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
            inlineStyle->removeProperty(CSSPropertyDirection);
            setNodeAttribute(*element, styleAttr, inlineStyle->asTextAtom(CSS::defaultSerializationContext()));
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(*element))
                removeNodePreservingChildren(*element);
        }
    }
}

static RefPtr<Node> highestEmbeddingAncestor(Node* startNode, Node* enclosingNode)
{
    for (RefPtr currentNode = startNode; currentNode && currentNode != enclosingNode; currentNode = currentNode->parentNode()) {
        if (currentNode->isHTMLElement() && valueID(Style::Extractor(currentNode.get()).propertyValue(CSSPropertyUnicodeBidi).get()) == CSSValueEmbed)
            return currentNode;
    }

    return nullptr;
}

void ApplyStyleCommand::applyInlineStyle(EditingStyle& style)
{
    RefPtr<ContainerNode> startDummySpanAncestor;
    RefPtr<ContainerNode> endDummySpanAncestor;

    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateLayoutIgnorePendingStylesheets();

    // adjust to the positions we want to use for applying style
    auto start = startPosition();
    auto end = endPosition();
    if (end < start)
        std::swap(start, end);

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = isValidCaretPositionInTextNode(start);
    if (splitStart) {
        if (shouldSplitTextElement(start.deprecatedNode()->parentElement(), style))
            splitTextElementAtStart(start, end);
        else
            splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
        if (start.isNull() || end.isNull())
            return;
        startDummySpanAncestor = dummySpanAncestorForNode(start.deprecatedNode());
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = isValidCaretPositionInTextNode(end);
    if (splitEnd) {
        if (shouldSplitTextElement(end.deprecatedNode()->parentElement(), style))
            splitTextElementAtEnd(start, end);
        else
            splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
        if (start.isNull() || end.isNull())
            return;
        endDummySpanAncestor = dummySpanAncestorForNode(end.deprecatedNode());
    }

    if (start.isNull() || start.isOrphan() || end.isNull() || end.isOrphan())
        return;

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    Position removeStart = start.upstream();
    auto textDirection = style.textDirection();
    RefPtr<EditingStyle> styleWithoutEmbedding;
    RefPtr<EditingStyle> embeddingStyle;
    if (textDirection) {
        // Leave alone an ancestor that provides the desired single level embedding, if there is one.
        auto startUnsplitAncestor = splitAncestorsWithUnicodeBidi(start.deprecatedNode(), true, *textDirection);
        auto endUnsplitAncestor = splitAncestorsWithUnicodeBidi(end.deprecatedNode(), false, *textDirection);
        removeEmbeddingUpToEnclosingBlock(start.deprecatedNode(), startUnsplitAncestor.get());
        removeEmbeddingUpToEnclosingBlock(end.deprecatedNode(), endUnsplitAncestor.get());

        // Avoid removing the dir attribute and the unicode-bidi and direction properties from the unsplit ancestors.
        Position embeddingRemoveStart = removeStart;
        if (startUnsplitAncestor && nodeFullySelected(*startUnsplitAncestor, removeStart, end))
            embeddingRemoveStart = positionInParentAfterNode(startUnsplitAncestor.get());

        Position embeddingRemoveEnd = end;
        if (endUnsplitAncestor && nodeFullySelected(*endUnsplitAncestor, removeStart, end))
            embeddingRemoveEnd = positionInParentBeforeNode(endUnsplitAncestor.get()).downstream();

        if (embeddingRemoveEnd != removeStart || embeddingRemoveEnd != end) {
            styleWithoutEmbedding = style.copy();
            embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();

            if (embeddingRemoveStart <= embeddingRemoveEnd)
                removeInlineStyle(*embeddingStyle, embeddingRemoveStart, embeddingRemoveEnd);
        }
    }

    removeInlineStyle(styleWithoutEmbedding ? *styleWithoutEmbedding : style, removeStart, end);
    start = startPosition();
    end = endPosition();
    if (start.isNull() || start.isOrphan() || end.isNull() || end.isOrphan())
        return;

    if (splitStart && mergeStartWithPreviousIfIdentical(start, end)) {
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;
    
    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateLayoutIgnorePendingStylesheets();

    RefPtr styleToApply = style;
    if (textDirection) {
        // Avoid applying the unicode-bidi and direction properties beneath ancestors that already have them.
        RefPtr startNode = start.deprecatedNode();
        auto embeddingStartNode = highestEmbeddingAncestor(startNode.get(), enclosingBlock(startNode).get());
        RefPtr endNode = end.deprecatedNode();
        auto embeddingEndNode = highestEmbeddingAncestor(endNode.get(), enclosingBlock(endNode).get());

        if (embeddingStartNode || embeddingEndNode) {
            Position embeddingApplyStart = embeddingStartNode ? positionInParentAfterNode(embeddingStartNode.get()) : start;
            Position embeddingApplyEnd = embeddingEndNode ? positionInParentBeforeNode(embeddingEndNode.get()) : end;
            ASSERT(embeddingApplyStart.isNotNull() && embeddingApplyEnd.isNotNull());

            if (!embeddingStyle) {
                styleWithoutEmbedding = style.copy();
                embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();
            }
            fixRangeAndApplyInlineStyle(*embeddingStyle, embeddingApplyStart, embeddingApplyEnd);

            styleToApply = styleWithoutEmbedding;
        }
    }

    fixRangeAndApplyInlineStyle(*styleToApply, start, end);

    // Remove dummy style spans created by splitting text elements.
    cleanupUnstyledAppleStyleSpans(startDummySpanAncestor.get());
    if (endDummySpanAncestor != startDummySpanAncestor)
        cleanupUnstyledAppleStyleSpans(endDummySpanAncestor.get());
}

void ApplyStyleCommand::fixRangeAndApplyInlineStyle(EditingStyle& style, const Position& start, const Position& end)
{
    RefPtr startNode = start.deprecatedNode();

    if (start.deprecatedEditingOffset() >= caretMaxOffset(*startNode)) {
        startNode = NodeTraversal::next(*startNode);
        if (!startNode || end < firstPositionInOrBeforeNode(startNode.get()))
            return;
    }

    RefPtr pastEndNode = end.deprecatedNode();
    if (end.deprecatedEditingOffset() >= caretMaxOffset(*pastEndNode))
        pastEndNode = NodeTraversal::nextSkippingChildren(*pastEndNode);

    // FIXME: Callers should perform this operation on a Range that includes the br
    // if they want style applied to the empty line.
    // FIXME: Should this be using startNode instead of start.deprecatedNode()?
    if (start == end && start.deprecatedNode()->hasTagName(brTag))
        pastEndNode = NodeTraversal::next(*start.deprecatedNode());

    // Start from the highest fully selected ancestor so that we can modify the fully selected node.
    // e.g. When applying font-size: large on <font color="blue">hello</font>, we need to include the font element in our run
    // to generate <font color="blue" size="4">hello</font> instead of <font color="blue"><font size="4">hello</font></font>
    auto range = *makeSimpleRange(start, end);
    RefPtr editableRoot { startNode->rootEditableElement() };
    if (startNode != editableRoot) {
        while (editableRoot && startNode->parentNode() != editableRoot && isNodeVisiblyContainedWithin(*startNode->parentNode(), range))
            startNode = startNode->parentNode();
    }

    applyInlineStyleToNodeRange(style, *startNode, pastEndNode.get());
}

static bool containsNonEditableRegion(Node& node)
{
    if (!node.hasEditableStyle())
        return true;

    RefPtr sibling { NodeTraversal::nextSkippingChildren(node) };
    for (RefPtr descendant = node.firstChild(); descendant && descendant != sibling; descendant = NodeTraversal::next(*descendant)) {
        if (!descendant->hasEditableStyle())
            return true;
    }

    return false;
}

struct InlineRunToApplyStyle {
    InlineRunToApplyStyle(Node* start, Node* end, Node* pastEndNode)
        : start(start)
        , end(end)
        , pastEndNode(pastEndNode)
    {
        ASSERT(start->parentNode() == end->parentNode());
    }

    bool startAndEndAreStillInDocument()
    {
        return start && end && start->isConnected() && end->isConnected();
    }

    RefPtr<Node> start;
    RefPtr<Node> end;
    RefPtr<Node> pastEndNode;
    Position positionForStyleComputation;
    RefPtr<Node> dummyElement;
    StyleChange change;
};

void ApplyStyleCommand::applyInlineStyleToNodeRange(EditingStyle& style, Node& startNode, Node* pastEndNode)
{
    if (m_removeOnly)
        return;

    document().updateLayoutIgnorePendingStylesheets();

    Vector<InlineRunToApplyStyle> runs;
    RefPtr node = startNode;
    for (RefPtr<Node> next; node && node != pastEndNode; node = next) {
        next = NodeTraversal::next(*node);

        if (!node->renderer() || !node->hasEditableStyle())
            continue;
        
        if (!node->hasRichlyEditableStyle()) {
            if (auto* element = dynamicDowncast<HTMLElement>(*node)) {
                // This is a plaintext-only region. Only proceed if it's fully selected.
                // pastEndNode is the node after the last fully selected node, so if it's inside node then
                // node isn't fully selected.
                if (pastEndNode && pastEndNode->isDescendantOf(*node))
                    break;
                // Add to this element's inline style and skip over its contents.
                RefPtr inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
                if (RefPtr otherStyle = style.style())
                    inlineStyle->mergeAndOverrideOnConflict(*otherStyle);
                setNodeAttribute(*element, styleAttr, inlineStyle->asTextAtom(CSS::defaultSerializationContext()));
                next = NodeTraversal::nextSkippingChildren(*element);
                continue;
            }
        }
        
        if (isBlock(*node))
            continue;
        
        if (node->hasChildNodes()) {
            if (node->contains(pastEndNode) || containsNonEditableRegion(*node) || !node->parentNode()->hasEditableStyle())
                continue;
            if (editingIgnoresContent(*node)) {
                next = NodeTraversal::nextSkippingChildren(*node);
                continue;
            }
        }

        auto runStart = node;
        auto runEnd = node;
        RefPtr sibling { node->nextSibling() };
        while (sibling && sibling != pastEndNode && !sibling->contains(pastEndNode) && (!isBlock(*sibling) || sibling->hasTagName(brTag)) && !containsNonEditableRegion(*sibling)) {
            runEnd = sibling;
            sibling = runEnd->nextSibling();
        }
        next = NodeTraversal::nextSkippingChildren(*runEnd);

        RefPtr pastEndNode { NodeTraversal::nextSkippingChildren(*runEnd) };
        if (!shouldApplyInlineStyleToRun(style, runStart.get(), pastEndNode.get()))
            continue;

        runs.append(InlineRunToApplyStyle(runStart.get(), runEnd.get(), pastEndNode.get()));
    }

    for (auto& run : runs) {
        removeConflictingInlineStyleFromRun(style, run.start, run.end, run.pastEndNode.get());
        if (run.startAndEndAreStillInDocument())
            run.positionForStyleComputation = positionToComputeInlineStyleChange(*run.start, run.dummyElement);
    }

    document().updateLayoutIgnorePendingStylesheets();

    for (auto& run : runs)
        run.change = StyleChange(&style, run.positionForStyleComputation);

    for (auto& run : runs) {
        if (run.dummyElement)
            removeNode(*run.dummyElement);
        if (run.startAndEndAreStillInDocument())
            applyInlineStyleChange(run.start.releaseNonNull(), run.end.releaseNonNull(), run.change, AddStyledElement::Yes);
    }
}

bool ApplyStyleCommand::isStyledInlineElementToRemove(Element* element) const
{
    return (m_styledInlineElement && element->hasTagName(m_styledInlineElement->tagQName()))
        || (m_isInlineElementToRemoveFunction && m_isInlineElementToRemoveFunction(element));
}

bool ApplyStyleCommand::shouldApplyInlineStyleToRun(EditingStyle& style, Node* runStart, Node* pastEndNode)
{
    ASSERT(runStart);

    for (RefPtr node = runStart; node && node != pastEndNode; node = NodeTraversal::next(*node)) {
        if (node->hasChildNodes())
            continue;
        // We don't consider m_isInlineElementToRemoveFunction here because we never apply style when m_isInlineElementToRemoveFunction is specified
        if (!style.styleIsPresentInComputedStyleOfNode(*node))
            return true;
        if (m_styledInlineElement && !enclosingElementWithTag(positionBeforeNode(node.get()), m_styledInlineElement->tagQName()))
            return true;
    }
    return false;
}

void ApplyStyleCommand::removeConflictingInlineStyleFromRun(EditingStyle& style, RefPtr<Node>& runStart, RefPtr<Node>& runEnd, Node* pastEndNode)
{
    ASSERT(runStart && runEnd);
    RefPtr<Node> next = runStart;
    for (RefPtr<Node> node = next; node && node->isConnected() && node != pastEndNode; node = next) {
        if (editingIgnoresContent(*node)) {
            ASSERT(!node->contains(pastEndNode));
            next = NodeTraversal::nextSkippingChildren(*node);
        } else
            next = NodeTraversal::next(*node);

        auto* htmlElement = dynamicDowncast<HTMLElement>(*node);
        if (!htmlElement)
            continue;

        RefPtr<Node> previousSibling = node->previousSibling();
        RefPtr<Node> nextSibling = node->nextSibling();
        RefPtr<ContainerNode> parent = node->parentNode();
        removeInlineStyleFromElement(style, *htmlElement, InlineStyleRemovalMode::Always);
        if (!node->isConnected()) {
            // FIXME: We might need to update the start and the end of current selection here but need a test.
            if (runStart == node)
                runStart = previousSibling ? previousSibling->nextSibling() : parent->firstChild();
            if (runEnd == node)
                runEnd = nextSibling ? nextSibling->previousSibling() : parent->lastChild();
        }
    }
}

bool ApplyStyleCommand::removeInlineStyleFromElement(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (!element.parentNode() || !isEditableNode(*element.parentNode()))
        return false;

    if (isStyledInlineElementToRemove(&element)) {
        if (mode == InlineStyleRemovalMode::None)
            return true;
        if (extractedStyle)
            extractedStyle->mergeInlineStyleOfElement(element, EditingStyle::OverrideValues);
        removeNodePreservingChildren(element);
        return true;
    }

    bool removed = false;
    if (removeImplicitlyStyledElement(style, element, mode, extractedStyle))
        removed = true;

    if (!element.isConnected())
        return removed;

    // If the node was converted to a span, the span may still contain relevant
    // styles which must be removed (e.g. <b style='font-weight: bold'>)
    if (removeCSSStyle(style, element, mode, extractedStyle))
        removed = true;

    return removed;
}
    
void ApplyStyleCommand::replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement& element)
{
    if (hasNoAttributeOrOnlyStyleAttribute(element, StyleAttributeShouldBeEmpty))
        removeNodePreservingChildren(element);
    else {
        RefPtr newSpanElement = replaceElementWithSpanPreservingChildrenAndAttributes(element);
        ASSERT_UNUSED(newSpanElement, newSpanElement && newSpanElement->isConnected());
    }
}

bool ApplyStyleCommand::removeImplicitlyStyledElement(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (mode == InlineStyleRemovalMode::None) {
        ASSERT(!extractedStyle);
        return style.conflictsWithImplicitStyleOfElement(element) || style.conflictsWithImplicitStyleOfAttributes(element);
    }

    ASSERT(mode == InlineStyleRemovalMode::IfNeeded || mode == InlineStyleRemovalMode::Always);
    if (style.conflictsWithImplicitStyleOfElement(element, extractedStyle, mode == InlineStyleRemovalMode::Always ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle)) {
        replaceWithSpanOrRemoveIfWithoutAttributes(element);
        return true;
    }

    // unicode-bidi and direction are pushed down separately so don't push down with other styles
    Vector<QualifiedName> attributes;
    if (!style.extractConflictingImplicitStyleOfAttributes(element, extractedStyle ? EditingStyle::PreserveWritingDirection : EditingStyle::DoNotPreserveWritingDirection, extractedStyle, attributes, mode == InlineStyleRemovalMode::Always ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle))
        return false;

    for (auto& attribute : attributes)
        removeNodeAttribute(element, attribute);

    if (isEmptyFontTag(&element) || isSpanWithoutAttributesOrUnstyledStyleSpan(element))
        removeNodePreservingChildren(element);

    return true;
}

bool ApplyStyleCommand::removeCSSStyle(EditingStyle& style, HTMLElement& element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    if (mode == InlineStyleRemovalMode::None)
        return style.conflictsWithInlineStyleOfElement(element);

    RefPtr<MutableStyleProperties> newInlineStyle;
    if (!style.conflictsWithInlineStyleOfElement(element, newInlineStyle, extractedStyle))
        return false;

    if (newInlineStyle->isEmpty())
        removeNodeAttribute(element, styleAttr);
    else
        setNodeAttribute(element, styleAttr, newInlineStyle->asTextAtom(CSS::defaultSerializationContext()));

    if (isSpanWithoutAttributesOrUnstyledStyleSpan(element))
        removeNodePreservingChildren(element);

    return true;
}

RefPtr<HTMLElement> ApplyStyleCommand::highestAncestorWithConflictingInlineStyle(EditingStyle& style, Node* node)
{
    if (!node)
        return nullptr;

    RefPtr<HTMLElement> result;
    RefPtr unsplittableElement = unsplittableElementForPosition(firstPositionInOrBeforeNode(node));

    for (RefPtr ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        auto* htmlAncestor = dynamicDowncast<HTMLElement>(*ancestor);
        if (htmlAncestor && shouldRemoveInlineStyleFromElement(style, *htmlAncestor))
            result = htmlAncestor;
        // Should stop at the editable root (cannot cross editing boundary) and
        // also stop at the unsplittable element to be consistent with other UAs
        if (ancestor == unsplittableElement)
            break;
    }

    return result;
}

void ApplyStyleCommand::applyInlineStyleToPushDown(Node& node, EditingStyle* style)
{
    node.protectedDocument()->updateStyleIfNeeded();

    if (!style || style->isEmpty() || !node.renderer() || is<HTMLIFrameElement>(node))
        return;

    RefPtr newInlineStyle = style;
    if (auto* htmlElement = dynamicDowncast<HTMLElement>(node); htmlElement && htmlElement->inlineStyle()) {
        newInlineStyle = style->copy();
        newInlineStyle->mergeInlineStyleOfElement(*htmlElement, EditingStyle::OverrideValues);
    }

    // Since addInlineStyleIfNeeded can't add styles to block-flow render objects, add style attribute instead.
    // FIXME: applyInlineStyleToRange should be used here instead.
    if (node.renderer()->isRenderBlockFlow() || node.hasChildNodes()) {
        if (auto* htmlElement = dynamicDowncast<HTMLElement>(node)) {
            setNodeAttribute(*htmlElement, styleAttr, newInlineStyle->style()->asTextAtom(CSS::defaultSerializationContext()));
            return;
        }
    }

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(*node.renderer()); textRenderer && textRenderer->containsOnlyCollapsibleWhitespace())
            return;
        if (CheckedPtr linebreak = dynamicDowncast<RenderLineBreak>(*node.renderer()); linebreak && !linebreak->style().preserveNewline())
            return;
    }

    // We can't wrap node with the styled element here because new styled element will never be removed if we did.
    // If we modified the child pointer in pushDownInlineStyleAroundNode to point to new style element
    // then we fall into an infinite loop where we keep removing and adding styled element wrapping node.
    addInlineStyleIfNeeded(newInlineStyle.get(), node, node, AddStyledElement::No);
}

void ApplyStyleCommand::pushDownInlineStyleAroundNode(EditingStyle& style, Node* targetNode)
{
    auto highestAncestor = highestAncestorWithConflictingInlineStyle(style, targetNode);
    if (!highestAncestor)
        return;

    // The outer loop is traversing the tree vertically from highestAncestor to targetNode
    RefPtr<Node> current = highestAncestor;
    // Along the way, styled elements that contain targetNode are removed and accumulated into elementsToPushDown.
    // Each child of the removed element, exclusing ancestors of targetNode, is then wrapped by clones of elements in elementsToPushDown.
    Vector<Ref<Element>> elementsToPushDown;
    while (current && current != targetNode && current->contains(targetNode)) {
        NodeVector currentChildren;
        collectChildNodes(*current, currentChildren);

        RefPtr<StyledElement> styledElement;
        if (RefPtr currentElement = dynamicDowncast<StyledElement>(*current); currentElement && isStyledInlineElementToRemove(currentElement.get())) {
            styledElement = WTFMove(currentElement);
            elementsToPushDown.append(*styledElement);
        }

        auto styleToPushDown = EditingStyle::create();
        if (auto* htmlElement = dynamicDowncast<HTMLElement>(*current))
            removeInlineStyleFromElement(style, *htmlElement, InlineStyleRemovalMode::IfNeeded, styleToPushDown.ptr());

        // The inner loop will go through children on each level
        // FIXME: we should aggregate inline child elements together so that we don't wrap each child separately.
        for (Ref<Node>& child : currentChildren) {
            if (!child->parentNode())
                continue;
            if (!child->contains(targetNode) && elementsToPushDown.size()) {
                for (auto& element : elementsToPushDown) {
                    Ref wrapper = element->cloneElementWithoutChildren(document(), nullptr);
                    wrapper->removeAttribute(styleAttr);
                    surroundNodeRangeWithElement(child, child, WTFMove(wrapper));
                }
            }

            // Apply style to all nodes containing targetNode and their siblings but NOT to targetNode
            // But if we've removed styledElement then always apply the style.
            if (child.ptr() != targetNode || styledElement)
                applyInlineStyleToPushDown(child, styleToPushDown.ptr());

            // We found the next node for the outer loop (contains targetNode)
            // When reached targetNode, stop the outer loop upon the completion of the current inner loop
            if (child.ptr() == targetNode || child->contains(targetNode))
                current = child.ptr();
        }
    }
}

void ApplyStyleCommand::removeInlineStyle(EditingStyle& style, const Position& start, const Position& end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.anchorNode()->isConnected());
    ASSERT(end.anchorNode()->isConnected());
    ASSERT(is_lteq(treeOrder<ShadowIncludingTree>(start, end)));
    // FIXME: We should assert that start/end are not in the middle of a text node.

    Position pushDownStart = start.downstream();
    // If the pushDownStart is at the end of a text node, then this node is not fully selected.
    // Move it to the next deep quivalent position to avoid removing the style from this node.
    // e.g. if pushDownStart was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead.
    RefPtr pushDownStartContainer { pushDownStart.containerNode() };
    if (RefPtr text = dynamicDowncast<Text>(pushDownStartContainer.get()); text && static_cast<unsigned>(pushDownStart.computeOffsetInContainerNode()) == text->length())
        pushDownStart = nextVisuallyDistinctCandidate(pushDownStart);
    // If pushDownEnd is at the start of a text node, then this node is not fully selected.
    // Move it to the previous deep equivalent position to avoid removing the style from this node.
    Position pushDownEnd = end.upstream();
    RefPtr pushDownEndContainer { pushDownEnd.containerNode() };
    if (is<Text>(pushDownEndContainer) && !pushDownEnd.computeOffsetInContainerNode())
        pushDownEnd = previousVisuallyDistinctCandidate(pushDownEnd);

    pushDownInlineStyleAroundNode(style, pushDownStart.deprecatedNode());
    pushDownInlineStyleAroundNode(style, pushDownEnd.deprecatedNode());

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    // If pushDownInlineStyleAroundNode has pruned start.deprecatedNode() or end.deprecatedNode(),
    // use pushDownStart or pushDownEnd instead, which pushDownInlineStyleAroundNode won't prune.
    Position s = start.isNull() || start.isOrphan() ? pushDownStart : start;
    Position e = end.isNull() || end.isOrphan() ? pushDownEnd : end;

    RefPtr node = start.deprecatedNode();
    while (node) {
        RefPtr<Node> next;
        if (editingIgnoresContent(*node)) {
            ASSERT(node == end.deprecatedNode() || !node->contains(end.deprecatedNode()));
            next = NodeTraversal::nextSkippingChildren(*node);
        } else
            next = NodeTraversal::next(*node);

        if (RefPtr element = dynamicDowncast<HTMLElement>(*node); element && nodeFullySelected(*element, start, end)) {
            RefPtr<Node> prev = NodeTraversal::previousPostOrder(*element);
            RefPtr<Node> next = NodeTraversal::next(*element);
            RefPtr<EditingStyle> styleToPushDown;
            RefPtr<Node> childNode;
            if (isStyledInlineElementToRemove(element.get())) {
                styleToPushDown = EditingStyle::create();
                childNode = element->firstChild();
            }

            removeInlineStyleFromElement(style, *element, InlineStyleRemovalMode::IfNeeded, styleToPushDown.get());
            if (!element->isConnected()) {
                if (s.deprecatedNode() == element.get()) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    ASSERT(s.anchorType() == Position::PositionIsBeforeAnchor || s.anchorType() == Position::PositionIsBeforeChildren || s.offsetInContainerNode() <= 0);
                    s = firstPositionInOrBeforeNode(next.get());
                }
                if (e.deprecatedNode() == element.get()) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    ASSERT(s.anchorType() == Position::PositionIsAfterAnchor || !offsetIsBeforeLastNodeOffset(s.offsetInContainerNode(), s.containerNode()));
                    e = lastPositionInOrAfterNode(prev.get());
                }
            }

            if (styleToPushDown) {
                for (; childNode; childNode = childNode->nextSibling())
                    applyInlineStyleToPushDown(*childNode, styleToPushDown.get());
            }
        }
        if (node == end.deprecatedNode())
            break;
        node = next.get();
    }

    updateStartEnd(s, e);
}

bool ApplyStyleCommand::nodeFullySelected(Element& element, const Position& start, const Position& end) const
{
    // The tree may have changed and Position::upstream() relies on an up-to-date layout.
    element.protectedDocument()->updateLayoutIgnorePendingStylesheets();
    return firstPositionInOrBeforeNode(&element) >= start && lastPositionInOrAfterNode(&element).upstream() <= end;
}

void ApplyStyleCommand::splitTextAtStart(const Position& start, const Position& end)
{
    ASSERT(is<Text>(start.containerNode()));

    Position newEnd;
    if (end.anchorType() == Position::PositionIsOffsetInAnchor && start.containerNode() == end.containerNode())
        newEnd = Position(end.containerText(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    RefPtr text = start.containerText();
    splitTextNode(*text, start.offsetInContainerNode());
    updateStartEnd(firstPositionInNode(text.get()), newEnd);
}

void ApplyStyleCommand::splitTextAtEnd(const Position& start, const Position& end)
{
    ASSERT(is<Text>(end.containerNode()));

    bool shouldUpdateStart = start.anchorType() == Position::PositionIsOffsetInAnchor && start.containerNode() == end.containerNode();
    Ref text = downcast<Text>(*end.deprecatedNode());
    splitTextNode(text, end.offsetInContainerNode());

    RefPtr prevNode = dynamicDowncast<Text>(text->previousSibling());
    if (!prevNode)
        return;

    Position newStart = shouldUpdateStart ? Position(prevNode.copyRef(), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, lastPositionInNode(prevNode.get()));
}

void ApplyStyleCommand::splitTextElementAtStart(const Position& start, const Position& end)
{
    ASSERT(is<Text>(start.containerNode()));

    Position newEnd;
    if (start.containerNode() == end.containerNode())
        newEnd = Position(end.containerText(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    splitTextNodeContainingElement(*start.protectedContainerText(), start.offsetInContainerNode());
    updateStartEnd(positionBeforeNode(start.protectedContainerNode().get()), newEnd);
}

void ApplyStyleCommand::splitTextElementAtEnd(const Position& start, const Position& end)
{
    ASSERT(is<Text>(end.containerNode()));

    bool shouldUpdateStart = start.containerNode() == end.containerNode();
    splitTextNodeContainingElement(*end.protectedContainerText(), end.offsetInContainerNode());

    RefPtr parentElement = end.containerNode()->parentNode();
    if (!parentElement || !parentElement->previousSibling())
        return;
    RefPtr firstTextNode = dynamicDowncast<Text>(parentElement->previousSibling()->lastChild());
    if (!firstTextNode)
        return;

    Position newStart = shouldUpdateStart ? Position(firstTextNode.copyRef(), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, positionAfterNode(firstTextNode.get()));
}

bool ApplyStyleCommand::shouldSplitTextElement(Element* element, EditingStyle& style)
{
    auto* htmlElement = dynamicDowncast<HTMLElement>(element);
    return htmlElement && shouldRemoveInlineStyleFromElement(style, *htmlElement);
}

bool ApplyStyleCommand::isValidCaretPositionInTextNode(const Position& position)
{
    ASSERT(position.isNotNull());
    
    RefPtr node = position.containerNode();
    if (position.anchorType() != Position::PositionIsOffsetInAnchor || !is<Text>(node))
        return false;
    int offsetInText = position.offsetInContainerNode();
    return offsetInText > caretMinOffset(*node) && offsetInText < caretMaxOffset(*node);
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position& start, const Position& end)
{
    RefPtr startNode { start.containerNode() };
    if (start.computeOffsetInContainerNode())
        return false;

    if (isAtomicNode(startNode.get())) {
        // note: prior siblings could be unrendered elements. it's silly to miss the
        // merge opportunity just for that.
        if (startNode->previousSibling())
            return false;

        startNode = startNode->parentNode();
    }

    RefPtr element = dynamicDowncast<Element>(*startNode);
    if (!element)
        return false;

    RefPtr previousSibling { startNode->previousSibling() };
    if (!previousSibling)
        return false;
    RefPtr previousElement = elementIfEquivalent(*element, *previousSibling);
    if (!previousElement)
        return false;

    RefPtr startChild = element->firstChild();
    ASSERT(startChild);
    mergeIdenticalElements(*previousElement, *element);

    // FIXME: Inconsistent that we use computeOffsetInContainerNode for start, but deprecatedEditingOffset for end.
    unsigned startOffset = startChild->computeNodeIndex();
    unsigned endOffset = end.deprecatedEditingOffset() + (startNode == end.deprecatedNode() ? startOffset : 0);
    updateStartEnd({ startNode.get(), startOffset, Position::PositionIsOffsetInAnchor },
        { end.protectedDeprecatedNode(), endOffset, Position::PositionIsOffsetInAnchor });
    return true;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position& start, const Position& end)
{
    RefPtr endNode { end.containerNode() };

    if (isAtomicNode(endNode.get())) {
        int endOffset = end.computeOffsetInContainerNode();
        if (offsetIsBeforeLastNodeOffset(endOffset, endNode.get()) || end.deprecatedNode()->nextSibling())
            return false;

        endNode = end.deprecatedNode()->parentNode();
    }

    if (endNode->hasTagName(brTag))
        return false;

    RefPtr element = dynamicDowncast<Element>(*endNode);
    if (!element)
        return false;

    RefPtr nextSibling { endNode->nextSibling() };
    if (!nextSibling)
        return false;
    RefPtr nextElement = elementIfEquivalent(*element, *nextSibling);
    if (!nextElement)
        return false;

    RefPtr nextChild = nextElement->firstChild();

    mergeIdenticalElements(*element, *nextElement);

    bool shouldUpdateStart = start.containerNode() == endNode;
    unsigned endOffset = nextChild ? nextChild->computeNodeIndex() : nextElement->countChildNodes();
    updateStartEnd(shouldUpdateStart ? Position(nextElement.copyRef(), start.offsetInContainerNode(), Position::PositionIsOffsetInAnchor) : start,
        { nextElement.copyRef(), endOffset, Position::PositionIsOffsetInAnchor });
    return true;
}

bool ApplyStyleCommand::surroundNodeRangeWithElement(Node& startNode, Node& endNode, Ref<Element>&& elementToInsert)
{
    Ref protectedStartNode = startNode;
    Ref element = WTFMove(elementToInsert);

    if (!insertNodeBefore(element.copyRef(), startNode) || !element->isContentRichlyEditable()) {
        removeNode(element);
        return false;
    }

    RefPtr node = startNode;
    while (node) {
        RefPtr next = node->nextSibling();
        if (isEditableNode(*node)) {
            removeNode(*node);
            appendNode(*node, element.copyRef());
        }
        if (node == &endNode)
            break;
        node = next;
    }

    RefPtr nextSibling = element->nextSibling();
    RefPtr previousSibling = element->previousSibling();

    if (nextSibling && nextSibling->hasEditableStyle()) {
        if (RefPtr nextElement = elementIfEquivalent(element, *nextSibling))
            mergeIdenticalElements(element, *nextElement);
    }

    if (RefPtr previousSiblingElement = dynamicDowncast<Element>(previousSibling.get()); previousSiblingElement && previousSiblingElement->hasEditableStyle()) {
        RefPtr mergedElementNode { previousSibling->nextSibling() };
        ASSERT(mergedElementNode);
        if (mergedElementNode->hasEditableStyle()) {
            if (RefPtr mergedElement = elementIfEquivalent(*previousSiblingElement, *mergedElementNode))
                mergeIdenticalElements(*previousSiblingElement, *mergedElement);
        }
    }

    // FIXME: We should probably call updateStartEnd if the start or end was in the node
    // range so that the endingSelection() is canonicalized.  See the comments at the end of
    // VisibleSelection::validate().
    return true;
}

static AtomString joinWithSpace(const String& a, const AtomString& b)
{
    if (a.isEmpty())
        return b;
    if (b.isEmpty())
        return AtomString { a };
    return makeAtomString(a, ' ', b);
}

void ApplyStyleCommand::addBlockStyle(const StyleChange& styleChange, HTMLElement& block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for inline content.
    ASSERT(styleChange.cssStyle());
    setNodeAttribute(block, styleAttr, joinWithSpace(styleChange.cssStyle()->asText(CSS::defaultSerializationContext()), block.getAttribute(styleAttr)));
}

void ApplyStyleCommand::addInlineStyleIfNeeded(EditingStyle* style, Node& start, Node& end, AddStyledElement addStyledElement)
{
    if (!start.isConnected() || !end.isConnected())
        return;

    Ref<Node> protectedStart = start;
    RefPtr<Node> dummyElement;
    StyleChange styleChange(style, positionToComputeInlineStyleChange(start, dummyElement));

    if (dummyElement)
        removeNode(*dummyElement);

    applyInlineStyleChange(start, end, styleChange, addStyledElement);
}

Position ApplyStyleCommand::positionToComputeInlineStyleChange(Node& startNode, RefPtr<Node>& dummyElement)
{
    // It's okay to obtain the style at the startNode because we've removed all relevant styles from the current run.
    if (!is<Element>(startNode)) {
        dummyElement = createStyleSpanElement(document());
        insertNodeAt(*dummyElement, positionBeforeNode(&startNode));
        return firstPositionInOrBeforeNode(dummyElement.get());
    }

    return firstPositionInOrBeforeNode(&startNode);
}

void ApplyStyleCommand::applyInlineStyleChange(Node& passedStart, Node& passedEnd, StyleChange& styleChange, AddStyledElement addStyledElement)
{
    RefPtr startNode = passedStart;
    RefPtr endNode = passedEnd;
    ASSERT(startNode->isConnected());
    ASSERT(endNode->isConnected());

    // Find appropriate font and span elements top-down.
    RefPtr<HTMLFontElement> fontContainer;
    RefPtr<HTMLElement> styleContainer;
    while (startNode == endNode) {
        if (auto* container = dynamicDowncast<HTMLElement>(*startNode)) {
            if (auto* fontElement = dynamicDowncast<HTMLFontElement>(*container))
                fontContainer = fontElement;
            if (is<HTMLSpanElement>(*container) || (!is<HTMLSpanElement>(styleContainer) && container->hasChildNodes()))
                styleContainer = container;
            if (!canHaveChildrenForEditing(*startNode))
                break;
        }
        RefPtr startNodeFirstChild = startNode->firstChild();
        if (!startNodeFirstChild)
            break;
        endNode = startNode->lastChild();
        startNode = startNodeFirstChild;
    }

    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        if (fontContainer) {
            if (styleChange.applyFontColor())
                setNodeAttribute(*fontContainer, colorAttr, styleChange.fontColor());
            if (styleChange.applyFontFace())
                setNodeAttribute(*fontContainer, faceAttr, styleChange.fontFace());
            if (styleChange.applyFontSize())
                setNodeAttribute(*fontContainer, sizeAttr, styleChange.fontSize());
        } else {
            auto fontElement = createFontElement(document());
            if (styleChange.applyFontColor())
                fontElement->setAttributeWithoutSynchronization(colorAttr, styleChange.fontColor());
            if (styleChange.applyFontFace())
                fontElement->setAttributeWithoutSynchronization(faceAttr, styleChange.fontFace());
            if (styleChange.applyFontSize())
                fontElement->setAttributeWithoutSynchronization(sizeAttr, styleChange.fontSize());
            surroundNodeRangeWithElement(*startNode, *endNode, WTFMove(fontElement));
        }
    }

    if (RefPtr styleToMerge = styleChange.cssStyle()) {
        if (styleContainer) {
            if (RefPtr existingStyle = styleContainer->inlineStyle()) {
                Ref inlineStyle = EditingStyle::create(existingStyle.get());
                inlineStyle->overrideWithStyle(*styleToMerge);
                setNodeAttribute(*styleContainer, styleAttr, inlineStyle->style()->asTextAtom(CSS::defaultSerializationContext()));
            } else
                setNodeAttribute(*styleContainer, styleAttr, styleToMerge->asTextAtom(CSS::defaultSerializationContext()));
        } else {
            auto styleElement = createStyleSpanElement(document());
            styleElement->setAttribute(styleAttr, styleToMerge->asTextAtom(CSS::defaultSerializationContext()));
            surroundNodeRangeWithElement(*startNode, *endNode, WTFMove(styleElement));
        }
    }

    if (styleChange.applyBold())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), bTag));

    if (styleChange.applyItalic())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), iTag));

    if (styleChange.applyUnderline())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), uTag));

    if (styleChange.applyLineThrough())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), strikeTag));

    if (styleChange.applySubscript())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), subTag));
    else if (styleChange.applySuperscript())
        surroundNodeRangeWithElement(*startNode, *endNode, createHTMLElement(document(), supTag));

    if (m_styledInlineElement && addStyledElement == AddStyledElement::Yes)
        surroundNodeRangeWithElement(*startNode, *endNode, m_styledInlineElement->cloneElementWithoutChildren(document(), nullptr));
}

float ApplyStyleCommand::computedFontSize(Node* node)
{
    if (!node)
        return 0;

    auto value = Style::Extractor(node).propertyValue(CSSPropertyFontSize);
    if (!value)
        return 0;
    return downcast<CSSPrimitiveValue>(*value).resolveAsLengthDeprecated();
}

void ApplyStyleCommand::joinChildTextNodes(Node* node, const Position& start, const Position& end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;

    Vector<Ref<Text>> textNodes;
    for (RefPtr textNode = TextNodeTraversal::firstChild(*node); textNode; textNode = TextNodeTraversal::nextSibling(*textNode))
        textNodes.append(*textNode);

    for (auto& childText : textNodes) {
        RefPtr next = dynamicDowncast<Text>(childText->nextSibling());
        if (!next)
            continue;

        if (start.anchorType() == Position::PositionIsOffsetInAnchor && next == start.containerNode())
            newStart = Position(childText.ptr(), childText->length() + start.offsetInContainerNode());
        if (end.anchorType() == Position::PositionIsOffsetInAnchor && next == end.containerNode())
            newEnd = Position(childText.ptr(), childText->length() + end.offsetInContainerNode());
        String textToMove = next->data();
        insertTextIntoNode(childText, childText->length(), textToMove);
        removeNode(*next);
        // don't move child node pointer. it may want to merge with more text nodes.
    }

    updateStartEnd(newStart, newEnd);
}

}
