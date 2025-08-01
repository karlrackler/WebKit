/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "VisibleSelection.h"

#include "BoundaryPointInlines.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "PositionInlines.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TextIterator.h"
#include "TreeScopeInlines.h"
#include "VisibleUnits.h"
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

const VisibleSelection& VisibleSelection::emptySelection()
{
    static NeverDestroyed<VisibleSelection> selection;
    return selection.get();
}

VisibleSelection::VisibleSelection() = default;

VisibleSelection::VisibleSelection(const Position& anchor, const Position& focus, Affinity affinity, Directionality directionality)
    : m_anchor(anchor)
    , m_focus(focus)
    , m_affinity(affinity)
    , m_directionality(directionality)
{
    validate();
}

VisibleSelection::VisibleSelection(const Position& position, Affinity affinity, Directionality directionality)
    : VisibleSelection(position, position, affinity, directionality)
{
}

VisibleSelection::VisibleSelection(const VisiblePosition& position, Directionality directionality)
    : VisibleSelection(position.deepEquivalent(), position.affinity(), directionality)
{
    // FIXME: Wasteful that this re-canonicalizes, but risky to change since the VisiblePosition object could be from before a mutation and its position may no longer be canonical.
}

VisibleSelection::VisibleSelection(const VisiblePosition& anchor, const VisiblePosition& focus, Directionality directionality)
    : VisibleSelection(anchor.deepEquivalent(), focus.deepEquivalent(), anchor.affinity(), directionality)
{
    // FIXME: Wasteful that this re-canonicalizes, but risky to change since the VisiblePosition objects could be from before a mutation and their positions may no longer be canonical.
}

VisibleSelection::VisibleSelection(const SimpleRange& range, Affinity affinity, Directionality directionality)
    : VisibleSelection(makeDeprecatedLegacyPosition(range.start), makeDeprecatedLegacyPosition(range.end), affinity, directionality)
{
}

VisibleSelection VisibleSelection::selectionFromContentsOfNode(Node* node)
{
    ASSERT(!editingIgnoresContent(*node));
    return VisibleSelection(VisiblePosition { firstPositionInNode(node) }, VisiblePosition { lastPositionInNode(node) });
}

const Position& VisibleSelection::uncanonicalizedStart() const
{
    return m_anchorIsFirst ? m_anchor : m_focus;
}

const Position& VisibleSelection::uncanonicalizedEnd() const
{
    return m_anchorIsFirst ? m_focus : m_anchor;
}

std::optional<SimpleRange> VisibleSelection::range() const
{
    auto start = uncanonicalizedStart();
    auto end = uncanonicalizedEnd();
    if (start.document())
        return makeSimpleRange(start, end);
    return makeSimpleRange(start.parentAnchoredEquivalent(), end.parentAnchoredEquivalent());
}

void VisibleSelection::setBase(const Position& position)
{
    m_anchor = position;
    validate();
}

void VisibleSelection::setBase(const VisiblePosition& visiblePosition)
{
    setBase(visiblePosition.deepEquivalent());
}

void VisibleSelection::setExtent(const Position& position)
{
    m_focus = position;
    validate();
}

void VisibleSelection::setExtent(const VisiblePosition& visiblePosition)
{
    setExtent(visiblePosition.deepEquivalent());
}

bool VisibleSelection::isOrphan() const
{
    if (m_base.isOrphan() || m_extent.isOrphan() || m_start.isOrphan() || m_end.isOrphan())
        return true;
    if (m_anchor.isOrphan())
        return true;
    if (m_focus.isOrphan())
        return true;
    return false;
}

RefPtr<Document> VisibleSelection::document() const
{
    RefPtr document { m_base.document() };
    if (!document) {
        document = m_anchor.document();
        if (!document)
            return nullptr;
    }

    if (m_extent.document() != document.get() || m_start.document() != document.get() || m_end.document() != document.get())
        return nullptr;

    if (m_anchor.document() != document.get() || m_focus.document() != document.get())
        return nullptr;

    return document;
}

std::optional<SimpleRange> VisibleSelection::firstRange() const
{
    if (isNoneOrOrphaned())
        return std::nullopt;
    // FIXME: Seems likely we don't need to call parentAnchoredEquivalent here.
    return makeSimpleRange(m_start.parentAnchoredEquivalent(), m_end.parentAnchoredEquivalent());
}

std::optional<SimpleRange> VisibleSelection::toNormalizedRange() const
{
    if (isNoneOrOrphaned())
        return std::nullopt;

    // Make sure we have an updated layout since this function is called
    // in the course of running edit commands which modify the DOM.
    // Failing to call this can result in equivalentXXXPosition calls returning
    // incorrect results.
    m_start.anchorNode()->protectedDocument()->updateLayout();

    // Check again, because updating layout can clear the selection.
    if (isNoneOrOrphaned())
        return std::nullopt;

    Position s, e;
    if (isCaret()) {
        // If the selection is a caret, move the range start upstream. This helps us match
        // the conventions of text editors tested, which make style determinations based
        // on the character before the caret, if any. 
        s = m_start.upstream().parentAnchoredEquivalent();
        e = s;
    } else {
        // If the selection is a range, select the minimum range that encompasses the selection.
        // Again, this is to match the conventions of text editors tested, which make style 
        // determinations based on the first character of the selection. 
        // For instance, this operation helps to make sure that the "X" selected below is the 
        // only thing selected. The range should not be allowed to "leak" out to the end of the 
        // previous text node, or to the beginning of the next text node, each of which has a 
        // different style.
        // 
        // On a treasure map, <b>X</b> marks the spot.
        //                       ^ selected
        //
        ASSERT(isRange());
        s = m_start.downstream().parentAnchoredEquivalent();
        e = m_end.upstream().parentAnchoredEquivalent();
        // Make sure the start is before the end.
        // The end can wind up before the start if collapsed whitespace is the only thing selected.
        if (s > e)
            std::swap(s, e);
    }

    return makeSimpleRange(s, e);
}

bool VisibleSelection::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    validate(granularity);
    return true;
}

bool VisibleSelection::isAll(EditingBoundaryCrossingRule rule) const
{
    return !nonBoundaryShadowTreeRootNode() && visibleStart().previous(rule).isNull() && visibleEnd().next(rule).isNull();
}

void VisibleSelection::appendTrailingWhitespace()
{
    RefPtr scope = deprecatedEnclosingBlockFlowElement(m_end.protectedDeprecatedNode().get());
    if (!scope)
        return;

    CharacterIterator charIt(*makeSimpleRange(m_end, makeBoundaryPointAfterNodeContents(*scope)), TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
    for (; !charIt.atEnd() && charIt.text().length(); charIt.advance(1)) {
        char16_t c = charIt.text()[0];
        if ((!deprecatedIsSpaceOrNewline(c) && c != noBreakSpace) || c == '\n')
            break;
        m_end = makeDeprecatedLegacyPosition(charIt.range().end);
        if (m_anchorIsFirst)
            m_focus = m_end;
        else
            m_anchor = m_end;
    }
}

void VisibleSelection::setBaseAndExtentToDeepEquivalents()
{
    // If only one of anchor and focus is null, convert to a caret selection.
    // FIXME: Seems like a better rule would be to convert to no selection.
    if (m_anchor.isNull())
        m_anchor = m_focus;
    if (m_focus.isNull())
        m_focus = m_anchor;

    m_anchorIsFirst = is_lteq(treeOrder<ShadowIncludingTree>(m_anchor, m_focus));

    m_base = VisiblePosition(m_anchor, m_affinity).deepEquivalent();
    if (m_anchor == m_focus)
        m_extent = m_base;
    else
        m_extent = VisiblePosition(m_focus, m_affinity).deepEquivalent();
    if (m_base.isNull() != m_extent.isNull()) {
        if (m_base.isNull())
            m_base = m_extent;
        else
            m_extent = m_base;
    }
}

void VisibleSelection::adjustSelectionRespectingGranularity(TextGranularity granularity)
{
    switch (granularity) {
        case TextGranularity::CharacterGranularity:
            // Don't do any expansion.
            break;
        case TextGranularity::WordGranularity: {
            // General case: Select the word the caret is positioned inside of, or at the start of (RightWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
            // the document, select that last word (LeftWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a paragraph, select from the end of the
            // last word to the line break (also RightWordIfOnBoundary);
            VisiblePosition start = VisiblePosition(m_start, m_affinity);
            VisiblePosition originalEnd(m_end, m_affinity);
            WordSide side = WordSide::RightWordIfOnBoundary;
            if (isEndOfEditableOrNonEditableContent(start) || (isEndOfLine(start) && !isStartOfLine(start) && !isEndOfParagraph(start)))
                side = WordSide::LeftWordIfOnBoundary;
            m_start = startOfWord(start, side).deepEquivalent();
            side = WordSide::RightWordIfOnBoundary;
            if (isEndOfEditableOrNonEditableContent(originalEnd) || (isEndOfLine(originalEnd) && !isStartOfLine(originalEnd) && !isEndOfParagraph(originalEnd)))
                side = WordSide::LeftWordIfOnBoundary;

            VisiblePosition wordEnd(endOfWord(originalEnd, side));
            VisiblePosition end(wordEnd);
            
            if (isEndOfParagraph(originalEnd) && !isEmptyTableCell(m_start.protectedDeprecatedNode().get())) {
                // Select the paragraph break (the space from the end of a paragraph to the start of 
                // the next one) to match TextEdit.
                end = wordEnd.next();
                
                if (RefPtr table = isFirstPositionAfterTable(end)) {
                    // The paragraph break after the last paragraph in the last cell of a block table ends
                    // at the start of the paragraph after the table.
                    if (isBlock(*table))
                        end = end.next(CannotCrossEditingBoundary);
                    else
                        end = wordEnd;
                }
                
                if (end.isNull())
                    end = wordEnd;
                    
            }
                
            m_end = end.deepEquivalent();

            // End must not be before start.
            if (m_start.deprecatedNode() == m_end.deprecatedNode() && m_start.deprecatedEditingOffset() > m_end.deprecatedEditingOffset())
                std::swap(m_start, m_end);
            break;
        }
        case TextGranularity::SentenceGranularity:
            m_start = startOfSentence(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfSentence(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case TextGranularity::LineGranularity: {
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            VisiblePosition end = endOfLine(VisiblePosition(m_end, m_affinity));
            // If the end of this line is at the end of a paragraph, include the space 
            // after the end of the line in the selection.
            if (isEndOfParagraph(end)) {
                VisiblePosition next = end.next();
                if (next.isNotNull())
                    end = next;
            }
            m_end = end.deepEquivalent();
            break;
        }
        case TextGranularity::LineBoundary:
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfLine(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case TextGranularity::ParagraphGranularity: {
            VisiblePosition position(m_start, m_affinity);
            if (isStartOfLine(position) && isEndOfEditableOrNonEditableContent(position))
                position = position.previous();
            m_start = startOfParagraph(position).deepEquivalent();
            VisiblePosition visibleParagraphEnd = endOfParagraph(VisiblePosition(m_end, m_affinity));
            
            // Include the "paragraph break" (the space from the end of this paragraph to the start
            // of the next one) in the selection.
            VisiblePosition end(visibleParagraphEnd.next());

            if (RefPtr table = isFirstPositionAfterTable(end)) {
                // The paragraph break after the last paragraph in the last cell of a block table ends
                // at the start of the paragraph after the table, not at the position just after the table.
                if (isBlock(*table))
                    end = end.next(CannotCrossEditingBoundary);
                // There is no parargraph break after the last paragraph in the last cell of an inline table.
                else
                    end = visibleParagraphEnd;
            }

            if (end.isNull())
                end = visibleParagraphEnd;

            m_end = end.deepEquivalent();
            break;
        }
        case TextGranularity::DocumentBoundary:
            m_start = startOfDocument(m_start.document()).deepEquivalent();
            m_end = endOfDocument(m_end.document()).deepEquivalent();
            break;
        case TextGranularity::ParagraphBoundary:
            m_start = startOfParagraph(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfParagraph(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case TextGranularity::SentenceBoundary:
            m_start = startOfSentence(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfSentence(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case TextGranularity::DocumentGranularity:
            ASSERT_NOT_REACHED();
            break;
    }
    
    // Make sure we do not have a dangling start or end.
    if (m_start.isNull())
        m_start = m_end;
    if (m_end.isNull())
        m_end = m_start;
}

void VisibleSelection::updateSelectionType()
{
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_type = Type::None;
        m_affinity = Affinity::Downstream;
    } else if (m_start == m_end || m_start.upstream() == m_end.upstream())
        m_type = Type::Caret;
    else {
        m_type = Type::Range;
        m_affinity = Affinity::Downstream;
    }
}

void VisibleSelection::validate(TextGranularity granularity)
{
    setBaseAndExtentToDeepEquivalents();

    m_start = m_anchorIsFirst ? m_base : m_extent;
    m_end = m_anchorIsFirst ? m_extent : m_base;

    auto startBeforeAdjustments = m_start;
    auto endBeforeAdjustments = m_end;

    adjustSelectionRespectingGranularity(granularity);
    adjustSelectionToAvoidCrossingShadowBoundaries();
    adjustSelectionToAvoidCrossingEditingBoundaries();
    updateSelectionType();

    bool shouldUpdateAnchor = m_start != startBeforeAdjustments;
    bool shouldUpdateFocus = m_end != endBeforeAdjustments;

    if (isRange()) {
        // "Constrain" the selection to be the smallest equivalent range of nodes.
        // This is a somewhat arbitrary choice, but experience shows that it is
        // useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a RANGE
        // come through here before anyone uses it.
        // FIXME: Canonicalizing is good, but haven't we already done it (when we
        // set these two positions to VisiblePosition deepEquivalent()s above)?
        m_start = m_start.downstream();
        m_end = m_end.upstream();

        // Position::downstream() or Position::upstream() might violate editing boundaries
        // if an anchor node has a Shadow DOM even though they should not. But because this
        // happens in practice, adjust selection to avoid crossing editing boundaries again.
        // See https://bugs.webkit.org/show_bug.cgi?id=87463.
        adjustSelectionToAvoidCrossingEditingBoundaries();
    }

    if (shouldUpdateAnchor) {
        m_anchor = m_anchorIsFirst ? m_start : m_end;
        m_base = m_anchor;
    }
    if (shouldUpdateFocus) {
        m_focus = m_anchorIsFirst ? m_end : m_start;
        m_extent = m_focus;
    }
}

// Because we use VisibleSelection to store values in editing commands for use when
// undoing the command, we need to be able to create a selection that, while currently
// invalid, will be valid once the changes are undone. This is a design problem.
// The best fix is likely to get rid of canonicalization from VisibleSelection entirely,
// and then remove this function.
void VisibleSelection::setWithoutValidation(const Position& anchor, const Position& focus)
{
    ASSERT(anchor.isNull() == focus.isNull());
    ASSERT(m_affinity == Affinity::Downstream);
    m_anchor = anchor;
    m_focus = focus;
    m_anchorIsFirst = is_lteq(treeOrder<ShadowIncludingTree>(m_anchor, m_focus));
    m_base = anchor;
    m_extent = focus;
    m_start = m_anchorIsFirst ? anchor : focus;
    m_end = m_anchorIsFirst ? focus : anchor;
    m_type = anchor == focus ? Type::Caret : Type::Range;
}

Position VisibleSelection::adjustPositionForEnd(const Position& currentPosition, Node* startContainerNode)
{
    TreeScope& treeScope = startContainerNode->treeScope();

    ASSERT(&currentPosition.containerNode()->treeScope() != &treeScope);

    if (RefPtr ancestor = treeScope.ancestorNodeInThisScope(currentPosition.protectedContainerNode().get())) {
        if (ancestor->contains(startContainerNode))
            return positionAfterNode(ancestor.get());
        return positionBeforeNode(ancestor.get());
    }

    if (RefPtr lastChild = treeScope.rootNode().lastChild())
        return positionAfterNode(lastChild.get());

    return Position();
}

Position VisibleSelection::adjustPositionForStart(const Position& currentPosition, Node* endContainerNode)
{
    TreeScope& treeScope = endContainerNode->treeScope();

    ASSERT(&currentPosition.containerNode()->treeScope() != &treeScope);
    
    if (RefPtr ancestor = treeScope.ancestorNodeInThisScope(currentPosition.protectedContainerNode().get())) {
        if (ancestor->contains(endContainerNode))
            return positionBeforeNode(ancestor.get());
        return positionAfterNode(ancestor.get());
    }

    if (RefPtr firstChild = treeScope.rootNode().firstChild())
        return positionBeforeNode(firstChild.get());

    return Position();
}

static bool isInUserAgentShadowRootOrHasEditableShadowAncestor(Node& node)
{
    RefPtr shadowRoot = node.containingShadowRoot();
    if (!shadowRoot)
        return false;

    if (shadowRoot->mode() == ShadowRootMode::UserAgent)
        return true;

    for (RefPtr currentNode = node; currentNode; currentNode = currentNode->parentOrShadowHostNode()) {
        if (currentNode->hasEditableStyle())
            return true;
    }
    return false;
}

void VisibleSelection::adjustSelectionToAvoidCrossingShadowBoundaries()
{
    if (m_start.isNull() || m_end.isNull())
        return;

    Ref startNode = *m_start.anchorNode();
    Ref endNode = *m_end.anchorNode();
    if (&startNode->treeScope() == &endNode->treeScope())
        return;

    if (!isInUserAgentShadowRootOrHasEditableShadowAncestor(startNode)
        && !isInUserAgentShadowRootOrHasEditableShadowAncestor(endNode))
        return;

    // Correct the focus if necessary.
    if (m_anchorIsFirst) {
        m_extent = adjustPositionForEnd(m_end, m_start.protectedContainerNode().get());
        m_end = m_extent;
    } else {
        m_extent = adjustPositionForStart(m_start, m_end.protectedContainerNode().get());
        m_start = m_extent;
    }
    m_focus = m_extent;
}

void VisibleSelection::adjustSelectionToAvoidCrossingEditingBoundaries()
{
    if (m_start.isNull() || m_end.isNull())
        return;

    // Early return in the caret case (the state hasn't actually been set yet, so we can't use isCaret()) to avoid the
    // expense of computing highestEditableRoot.
    if (m_base == m_start && m_base == m_end)
        return;

    auto baseRoot = highestEditableRoot(m_base);
    auto startRoot = highestEditableRoot(m_start);
    auto endRoot = highestEditableRoot(m_end);
    
    RefPtr baseEditableAncestor = lowestEditableAncestor(m_base.protectedContainerNode().get());
    
    // The base, start and end are all in the same region.  No adjustment necessary.
    if (baseRoot == startRoot && baseRoot == endRoot)
        return;
    
    // The selection is based in editable content.
    if (baseRoot) {
        // If the start is outside the base's editable root, cap it at the start of that root.
        // If the start is in non-editable content that is inside the base's editable root, put it
        // at the first editable position after start inside the base's editable root.
        if (startRoot != baseRoot) {
            VisiblePosition first = firstEditablePositionAfterPositionInRoot(m_start, baseRoot.get());
            m_start = first.deepEquivalent();
            if (m_start.isNull()) {
                ASSERT_NOT_REACHED();
                m_start = m_end;
            }
        }
        // If the end is outside the base's editable root, cap it at the end of that root.
        // If the end is in non-editable content that is inside the base's root, put it
        // at the last editable position before the end inside the base's root.
        if (endRoot != baseRoot) {
            VisiblePosition last = lastEditablePositionBeforePositionInRoot(m_end, baseRoot.get());
            m_end = last.deepEquivalent();
            if (m_end.isNull())
                m_end = m_start;
        }
    // The selection is based in non-editable content.
    } else {
        // FIXME: Non-editable pieces inside editable content should be atomic, in the same way that editable
        // pieces in non-editable content are atomic.
    
        // The selection ends in editable content or non-editable content inside a different editable ancestor, 
        // move backward until non-editable content inside the same lowest editable ancestor is reached.
        RefPtr endEditableAncestor = lowestEditableAncestor(m_end.protectedContainerNode().get());
        if (endRoot || endEditableAncestor != baseEditableAncestor) {
            
            Position p = previousVisuallyDistinctCandidate(m_end);
            RefPtr shadowAncestor = endRoot ? endRoot->shadowHost() : nullptr;
            if (p.isNull() && shadowAncestor)
                p = positionAfterNode(shadowAncestor.get());
            while (p.isNotNull() && !(lowestEditableAncestor(p.protectedContainerNode().get()) == baseEditableAncestor && !isEditablePosition(p))) {
                RefPtr root = editableRootForPosition(p);
                shadowAncestor = root ? root->shadowHost() : nullptr;
                p = isAtomicNode(p.protectedContainerNode().get()) ? positionInParentBeforeNode(p.protectedContainerNode().get()) : previousVisuallyDistinctCandidate(p);
                if (p.isNull() && shadowAncestor)
                    p = positionAfterNode(shadowAncestor.get());
            }
            VisiblePosition previous(p);

            if (previous.isNull()) {
                *this = { };
                return;
            }
            m_end = previous.deepEquivalent();
        }

        // The selection starts in editable content or non-editable content inside a different editable ancestor, 
        // move forward until non-editable content inside the same lowest editable ancestor is reached.
        RefPtr startEditableAncestor = lowestEditableAncestor(m_start.protectedContainerNode().get());
        if (startRoot || startEditableAncestor != baseEditableAncestor) {
            Position p = nextVisuallyDistinctCandidate(m_start);
            RefPtr shadowAncestor = startRoot ? startRoot->shadowHost() : nullptr;
            if (p.isNull() && shadowAncestor)
                p = positionBeforeNode(shadowAncestor.get());
            while (p.isNotNull() && !(lowestEditableAncestor(p.protectedContainerNode().get()) == baseEditableAncestor && !isEditablePosition(p))) {
                RefPtr root = editableRootForPosition(p);
                shadowAncestor = root ? root->shadowHost() : nullptr;
                p = isAtomicNode(p.protectedContainerNode().get()) ? positionInParentAfterNode(p.protectedContainerNode().get()) : nextVisuallyDistinctCandidate(p);
                if (p.isNull() && shadowAncestor)
                    p = positionBeforeNode(shadowAncestor.get());
            }
            VisiblePosition next(p);
            
            if (next.isNull()) {
                *this = { };
                return;
            }
            m_start = next.deepEquivalent();
        }
    }
    
    // Correct the focus if necessary.
    if (baseEditableAncestor != lowestEditableAncestor(m_extent.protectedContainerNode().get())) {
        m_extent = m_anchorIsFirst ? m_end : m_start;
        m_focus = m_extent;
    }
}

bool VisibleSelection::isContentEditable() const
{
    return isEditablePosition(start());
}

bool VisibleSelection::hasEditableStyle() const
{
    if (RefPtr containerNode = start().containerNode())
        return containerNode->hasEditableStyle();
    return false;
}

bool VisibleSelection::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(start());
}

Element* VisibleSelection::rootEditableElement() const
{
    return editableRootForPosition(start());
}

Node* VisibleSelection::nonBoundaryShadowTreeRootNode() const
{
    return start().deprecatedNode() && !start().deprecatedNode()->isShadowRoot() ? start().deprecatedNode()->nonBoundaryShadowTreeRootNode() : nullptr;
}

bool VisibleSelection::isInPasswordField() const
{
    RefPtr textControl = dynamicDowncast<HTMLInputElement>(enclosingTextFormControl(start()));
    return textControl && textControl->isPasswordField();
}

bool VisibleSelection::canEnableWritingSuggestions() const
{
    if (RefPtr formControl = enclosingTextFormControl(start()))
        return formControl->isWritingSuggestionsEnabled();

    RefPtr containerNode = start().containerNode();
    if (!containerNode)
        return false;

    if (RefPtr element = dynamicDowncast<Element>(containerNode.get()))
        return element->isWritingSuggestionsEnabled();

    if (RefPtr element = containerNode->parentElement())
        return element->isWritingSuggestionsEnabled();

    return false;
}

bool VisibleSelection::isInAutoFilledAndViewableField() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(enclosingTextFormControl(start())))
        return input->autofilledAndViewable();
    return false;
}

#if ENABLE(TREE_DEBUGGING)

void VisibleSelection::debugPosition() const
{
    fprintf(stderr, "VisibleSelection ===============\n");

    if (!m_start.anchorNode())
        SAFE_FPRINTF(stderr, "pos:   null");
    else if (m_start == m_end) {
        SAFE_FPRINTF(stderr, "pos:   %s ", m_start.anchorNode()->nodeName().utf8());
        m_start.showAnchorTypeAndOffset();
    } else {
        SAFE_FPRINTF(stderr, "start: %s ", m_start.anchorNode()->nodeName().utf8());
        m_start.showAnchorTypeAndOffset();
        SAFE_FPRINTF(stderr, "end:   %s ", m_end.anchorNode()->nodeName().utf8());
        m_end.showAnchorTypeAndOffset();
    }

    fprintf(stderr, "================================\n");
}

String VisibleSelection::debugDescription() const
{
    if (isNone())
        return "<none>"_s;
    return makeString("from "_s, start().debugDescription(), " to "_s, end().debugDescription());
}

void VisibleSelection::showTreeForThis() const
{
    if (RefPtr startAnchorNode = start().anchorNode()) {
        startAnchorNode->showTreeAndMark(startAnchorNode.get(), "S"_s, end().protectedAnchorNode().get(), "E"_s);
        SAFE_FPRINTF(stderr, "start: ");
        start().showAnchorTypeAndOffset();
        SAFE_FPRINTF(stderr, "end: ");
        end().showAnchorTypeAndOffset();
    }
}

#endif

TextStream& operator<<(TextStream& ts, const VisibleSelection& v)
{
    TextStream::GroupScope scope(ts);
    ts << "VisibleSelection "_s << &v;

    ts.dumpProperty("base"_s, v.base());
    ts.dumpProperty("extent"_s, v.extent());
    ts.dumpProperty("start"_s, v.start());
    ts.dumpProperty("end"_s, v.end());

    return ts;
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::VisibleSelection& sel)
{
    sel.showTreeForThis();
}

void showTree(const WebCore::VisibleSelection* sel)
{
    if (sel)
        sel->showTreeForThis();
}

#endif
