/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2020 Google  Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderFragmentContainer.h"

#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "LayoutRepainter.h"
#include "Range.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "StyleResolver.h"
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderFragmentContainer);

RenderFragmentContainer::RenderFragmentContainer(Type type, Element& element, RenderStyle&& style, RenderFragmentedFlow* fragmentedFlow)
    : RenderBlockFlow(type, element, WTFMove(style), BlockFlowFlag::IsFragmentContainer)
    , m_fragmentedFlow(fragmentedFlow)
{
}

RenderFragmentContainer::RenderFragmentContainer(Type type, Document& document, RenderStyle&& style, RenderFragmentedFlow* fragmentedFlow)
    : RenderBlockFlow(type, document, WTFMove(style), BlockFlowFlag::IsFragmentContainer)
    , m_fragmentedFlow(fragmentedFlow)
{
}

RenderFragmentContainer::~RenderFragmentContainer() = default;

LayoutPoint RenderFragmentContainer::mapFragmentPointIntoFragmentedFlowCoordinates(const LayoutPoint& point)
{
    // Assuming the point is relative to the fragment block, 3 cases will be considered:
    // a) top margin, padding or border.
    // b) bottom margin, padding or border.
    // c) non-content fragment area.

    LayoutUnit pointLogicalTop(isHorizontalWritingMode() ? point.y() : point.x());
    LayoutUnit pointLogicalLeft(isHorizontalWritingMode() ? point.x() : point.y());
    LayoutUnit fragmentedFlowLogicalTop(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.y() : m_fragmentedFlowPortionRect.x());
    LayoutUnit fragmentedFlowLogicalLeft(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.x() : m_fragmentedFlowPortionRect.y());
    LayoutUnit fragmentedFlowPortionTopBound(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.height() : m_fragmentedFlowPortionRect.width());
    LayoutUnit fragmentedFlowPortionLeftBound(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.width() : m_fragmentedFlowPortionRect.height());
    LayoutUnit fragmentedFlowPortionTopMax(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.maxY() : m_fragmentedFlowPortionRect.maxX());
    LayoutUnit fragmentedFlowPortionLeftMax(isHorizontalWritingMode() ? m_fragmentedFlowPortionRect.maxX() : m_fragmentedFlowPortionRect.maxY());
    LayoutUnit effectiveFixedPointDenominator;
    effectiveFixedPointDenominator.setRawValue(1);

    if (pointLogicalTop < 0) {
        LayoutPoint pointInThread(0_lu, fragmentedFlowLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }

    if (pointLogicalTop >= fragmentedFlowPortionTopBound) {
        LayoutPoint pointInThread(fragmentedFlowPortionLeftBound, fragmentedFlowPortionTopMax - effectiveFixedPointDenominator);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }

    if (pointLogicalLeft < 0) {
        LayoutPoint pointInThread(fragmentedFlowLogicalLeft, pointLogicalTop + fragmentedFlowLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }
    if (pointLogicalLeft >= fragmentedFlowPortionLeftBound) {
        LayoutPoint pointInThread(fragmentedFlowPortionLeftMax - effectiveFixedPointDenominator, pointLogicalTop + fragmentedFlowLogicalTop);
        return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
    }
    LayoutPoint pointInThread(pointLogicalLeft + fragmentedFlowLogicalLeft, pointLogicalTop + fragmentedFlowLogicalTop);
    return isHorizontalWritingMode() ? pointInThread : pointInThread.transposedPoint();
}

VisiblePosition RenderFragmentContainer::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    if (!isValid() || !m_fragmentedFlow->firstChild()) // checking for empty fragment blocks.
        return RenderBlock::positionForPoint(point, source, fragment);

    return m_fragmentedFlow->positionForPoint(mapFragmentPointIntoFragmentedFlowCoordinates(point), source, this);
}

LayoutUnit RenderFragmentContainer::pageLogicalWidth() const
{
    ASSERT(isValid());
    return m_fragmentedFlow->isHorizontalWritingMode() ? contentBoxWidth() : contentBoxHeight();
}

LayoutUnit RenderFragmentContainer::pageLogicalHeight() const
{
    ASSERT(isValid());
    return m_fragmentedFlow->isHorizontalWritingMode() ? contentBoxHeight() : contentBoxWidth();
}

LayoutUnit RenderFragmentContainer::logicalHeightOfAllFragmentedFlowContent() const
{
    return pageLogicalHeight();
}

LayoutRect RenderFragmentContainer::fragmentedFlowPortionOverflowRect() const
{
    return overflowRectForFragmentedFlowPortion(fragmentedFlowPortionRect(), isFirstFragment(), isLastFragment());
}

LayoutPoint RenderFragmentContainer::fragmentedFlowPortionLocation() const
{
    LayoutPoint portionLocation;
    LayoutRect portionRect = fragmentedFlowPortionRect();

    if (fragmentedFlow()->writingMode().isBlockFlipped()) {
        LayoutRect flippedFragmentedFlowPortionRect(portionRect);
        fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionRect);
        portionLocation = flippedFragmentedFlowPortionRect.location();
    } else
        portionLocation = portionRect.location();

    return portionLocation;
}

LayoutRect RenderFragmentContainer::overflowRectForFragmentedFlowPortion(const LayoutRect& fragmentedFlowPortionRect, bool isFirstPortion, bool isLastPortion) const
{
    ASSERT(isValid());
    if (shouldClipFragmentedFlowContent())
        return fragmentedFlowPortionRect;

    LayoutRect fragmentedFlowOverflow = visualOverflowRectForBox(*m_fragmentedFlow);
    LayoutRect clipRect;
    if (m_fragmentedFlow->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstPortion ? fragmentedFlowOverflow.y() : fragmentedFlowPortionRect.y();
        LayoutUnit maxY = isLastPortion ? std::max(fragmentedFlowPortionRect.maxY(), fragmentedFlowOverflow.maxY()) : fragmentedFlowPortionRect.maxY();
        bool clipX = effectiveOverflowX() != Overflow::Visible;
        LayoutUnit minX = clipX ? fragmentedFlowPortionRect.x() : std::min(fragmentedFlowPortionRect.x(), fragmentedFlowOverflow.x());
        LayoutUnit maxX = clipX ? fragmentedFlowPortionRect.maxX() : std::max(fragmentedFlowPortionRect.maxX(), fragmentedFlowOverflow.maxX());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstPortion ? fragmentedFlowOverflow.x() : fragmentedFlowPortionRect.x();
        LayoutUnit maxX = isLastPortion ? std::max(fragmentedFlowPortionRect.maxX(), fragmentedFlowOverflow.maxX()) : fragmentedFlowPortionRect.maxX();
        bool clipY = effectiveOverflowY() != Overflow::Visible;
        LayoutUnit minY = clipY ? fragmentedFlowPortionRect.y() : std::min(fragmentedFlowPortionRect.y(), fragmentedFlowOverflow.y());
        LayoutUnit maxY = clipY ? fragmentedFlowPortionRect.maxY() : std::max(fragmentedFlowPortionRect.y(), fragmentedFlowOverflow.maxY());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }
    return clipRect;
}

LayoutUnit RenderFragmentContainer::pageLogicalTopForOffset(LayoutUnit /* offset */) const
{
    return fragmentedFlow()->isHorizontalWritingMode() ? fragmentedFlowPortionRect().y() : fragmentedFlowPortionRect().x();
}

bool RenderFragmentContainer::isFirstFragment() const
{
    ASSERT(isValid());

    return m_fragmentedFlow->firstFragment() == this;
}

bool RenderFragmentContainer::isLastFragment() const
{
    ASSERT(isValid());

    return m_fragmentedFlow->lastFragment() == this;
}

bool RenderFragmentContainer::shouldClipFragmentedFlowContent() const
{
    return hasNonVisibleOverflow();
}

void RenderFragmentContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (!isValid())
        return;

    if (oldStyle && oldStyle->writingMode().computedWritingMode() !=writingMode().computedWritingMode())
        m_fragmentedFlow->fragmentChangedWritingMode(this);
}

void RenderFragmentContainer::repaintFragmentedFlowContent(const LayoutRect& repaintRect) const
{
    repaintFragmentedFlowContentRectangle(repaintRect, fragmentedFlowPortionRect(), contentBoxRect().location());
}

void RenderFragmentContainer::repaintFragmentedFlowContentRectangle(const LayoutRect& repaintRect, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect) const
{
    ASSERT(isValid());

    // We only have to issue a repaint in this fragment if the fragment rect intersects the repaint rect.
    LayoutRect clippedRect(repaintRect);

    if (fragmentedFlowPortionClipRect) {
        LayoutRect flippedFragmentedFlowPortionClipRect(*fragmentedFlowPortionClipRect);
        fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionClipRect);
        clippedRect.intersect(flippedFragmentedFlowPortionClipRect);
    }

    if (clippedRect.isEmpty())
        return;

    LayoutRect flippedFragmentedFlowPortionRect(fragmentedFlowPortionRect);
    fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionRect); // Put the fragment rects into physical coordinates.

    // Put the fragment rect into the fragment's physical coordinate space.
    clippedRect.setLocation(fragmentLocation + (clippedRect.location() - flippedFragmentedFlowPortionRect.location()));

    // Now switch to the fragment's writing mode coordinate space and let it repaint itself.
    flipForWritingMode(clippedRect);
    
    // Issue the repaint.
    repaintRectangle(clippedRect);
}

LayoutRect RenderFragmentContainer::fragmentedFlowContentRectangle(const LayoutRect& rect, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect) const
{
    auto clippedRect = rect;

    if (fragmentedFlowPortionClipRect) {
        LayoutRect flippedFragmentedFlowPortionClipRect(*fragmentedFlowPortionClipRect);
        fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionClipRect);
        clippedRect.edgeInclusiveIntersect(flippedFragmentedFlowPortionClipRect); // edgeInclusiveIntersect to avoid rects with zero height or width becoming zero-sized.
    }

    LayoutRect flippedFragmentedFlowPortionRect(fragmentedFlowPortionRect);
    fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionRect);

    // Put the fragment rect into the fragment's physical coordinate space.
    clippedRect.setLocation(fragmentLocation + (clippedRect.location() - flippedFragmentedFlowPortionRect.location()));

    // Now switch to the fragment's writing mode coordinate space and let it repaint itself.
    flipForWritingMode(clippedRect);
    
    return clippedRect;
}

Vector<LayoutRect> RenderFragmentContainer::fragmentRectsForFlowContentRect(const LayoutRect& contentRect) const
{
    auto portionRect = fragmentedFlowPortionRect();
    auto fragmentLocation = contentBoxRect().location();
    
    auto fragmentRect = contentRect;
    
    auto flippedFragmentedFlowPortionRect = portionRect;
    fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowPortionRect);
    fragmentRect.setLocation(fragmentLocation + (fragmentRect.location() - flippedFragmentedFlowPortionRect.location()));

    flipForWritingMode(fragmentRect);

    return { fragmentRect };
}

void RenderFragmentContainer::installFragmentedFlow()
{
    ASSERT_NOT_REACHED();
}

void RenderFragmentContainer::attachFragment()
{
    if (renderTreeBeingDestroyed())
        return;
    
    // A fragment starts off invalid.
    setIsValid(false);

    // Initialize the flow thread reference and create the flow thread object if needed.
    // The flow thread lifetime is influenced by the number of fragments attached to it,
    // and we are attaching the fragment to the flow thread.
    installFragmentedFlow();
    
    if (!m_fragmentedFlow)
        return;

    // Only after adding the fragment to the thread, the fragment is marked to be valid.
    m_fragmentedFlow->addFragmentToThread(this);
}

void RenderFragmentContainer::detachFragment()
{
    if (m_fragmentedFlow)
        m_fragmentedFlow->removeFragmentFromThread(*this);
    m_fragmentedFlow = nullptr;
}

RenderBoxFragmentInfo* RenderFragmentContainer::renderBoxFragmentInfo(const RenderBox& box) const
{
    ASSERT(isValid());
    return m_renderBoxFragmentInfo.get(box);
}

RenderBoxFragmentInfo* RenderFragmentContainer::setRenderBoxFragmentInfo(const RenderBox& box, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
    bool containingBlockChainIsInset)
{
    ASSERT(isValid());

    std::unique_ptr<RenderBoxFragmentInfo>& boxInfo = m_renderBoxFragmentInfo.add(box, makeUnique<RenderBoxFragmentInfo>(logicalLeftInset, logicalRightInset, containingBlockChainIsInset)).iterator->value;
    return boxInfo.get();
}

std::unique_ptr<RenderBoxFragmentInfo> RenderFragmentContainer::takeRenderBoxFragmentInfo(const RenderBox& box)
{
    return m_renderBoxFragmentInfo.take(box);
}

void RenderFragmentContainer::removeRenderBoxFragmentInfo(const RenderBox& box)
{
    m_renderBoxFragmentInfo.remove(box);
}

void RenderFragmentContainer::deleteAllRenderBoxFragmentInfo()
{
    m_renderBoxFragmentInfo.clear();
}

LayoutUnit RenderFragmentContainer::logicalTopOfFragmentedFlowContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return fragmentedFlow()->isHorizontalWritingMode() ? rect.y() : rect.x();
}

LayoutUnit RenderFragmentContainer::logicalBottomOfFragmentedFlowContentRect(const LayoutRect& rect) const
{
    ASSERT(isValid());
    return fragmentedFlow()->isHorizontalWritingMode() ? rect.maxY() : rect.maxX();
}

void RenderFragmentContainer::insertedIntoTree()
{
    attachFragment();
    if (isValid())
        RenderBlockFlow::insertedIntoTree();
}

void RenderFragmentContainer::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();

    detachFragment();
}

void RenderFragmentContainer::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!isValid()) {
        RenderBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }
    maxLogicalWidth = { };
    minLogicalWidth = { };
}

void RenderFragmentContainer::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    if (!isValid()) {
        RenderBlockFlow::computePreferredLogicalWidths();
        return;
    }

    // FIXME: Currently, the code handles only the <length> case for min-width/max-width.
    // It should also support other values, like percentage, calc or viewport relative.
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    auto& styleToUse = style();
    if (auto fixedLogicalWidth = styleToUse.logicalWidth().tryFixed(); fixedLogicalWidth && fixedLogicalWidth->value > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), borderAndPaddingLogicalWidth());

    clearNeedsPreferredWidthsUpdate();
}

LayoutRect RenderFragmentContainer::computedVisualOverflowRectForBox(const RenderBox& box) const
{
    ASSERT(m_fragmentedFlow->objectShouldFragmentInFlowFragment(&box, this));

    auto borderBox = box.borderBoxRect();
    if (borderBox.isEmpty())
        return { };

    borderBox = rectFlowPortionForBox(box, borderBox);
    m_fragmentedFlow->flipForWritingModeLocalCoordinates(borderBox);
    return borderBox;
}

LayoutRect RenderFragmentContainer::computedLayoutOverflowRectForBox(const RenderBox& box) const
{
    ASSERT(m_fragmentedFlow->objectShouldFragmentInFlowFragment(&box, this));
    auto clientBox = box.clientBoxRect();
    if (clientBox.isEmpty())
        return { };

    clientBox = rectFlowPortionForBox(box, clientBox);
    m_fragmentedFlow->flipForWritingModeLocalCoordinates(clientBox);
    return clientBox;
}

RenderOverflow* RenderFragmentContainer::overflowForBox(const RenderBox& box) const
{
    ASSERT(m_fragmentedFlow->renderFragmentContainerList().contains(*this));
    ASSERT(isValid());

    auto* boxInfo = renderBoxFragmentInfo(box);
    if (!boxInfo)
        return { };

    if (CheckedPtr overflow = boxInfo->overflow())
        return overflow.get();

    boxInfo->createOverflow(computedLayoutOverflowRectForBox(box), computedVisualOverflowRectForBox(box));
    return boxInfo->overflow();
}

LayoutRect RenderFragmentContainer::rectFlowPortionForBox(const RenderBox& box, const LayoutRect& rect) const
{
    LayoutRect mappedRect = m_fragmentedFlow->mapFromLocalToFragmentedFlow(&box, rect);

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    if (m_fragmentedFlow->getFragmentRangeForBox(box, startFragment, endFragment)) {
        if (fragmentedFlow()->isHorizontalWritingMode()) {
            if (this != startFragment)
                mappedRect.shiftYEdgeTo(std::max<LayoutUnit>(logicalTopForFragmentedFlowContent(), mappedRect.y()));
            if (this != endFragment)
                mappedRect.setHeight(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFragmentedFlowContent() - mappedRect.y(), mappedRect.height())));
        } else {
            if (this != startFragment)
                mappedRect.shiftXEdgeTo(std::max<LayoutUnit>(logicalTopForFragmentedFlowContent(), mappedRect.x()));
            if (this != endFragment)
                mappedRect.setWidth(std::max<LayoutUnit>(0, std::min<LayoutUnit>(logicalBottomForFragmentedFlowContent() - mappedRect.x(), mappedRect.width())));
        }
    }

    return m_fragmentedFlow->mapFromFragmentedFlowToLocal(&box, mappedRect);
}

void RenderFragmentContainer::addLayoutOverflowForBox(const RenderBox& box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    if (CheckedPtr overflow = overflowForBox(box))
        overflow->addLayoutOverflow(rect);
}

void RenderFragmentContainer::addVisualOverflowForBox(const RenderBox& box, const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    if (CheckedPtr overflow = overflowForBox(box)) {
        LayoutRect flippedRect = rect;
        fragmentedFlow()->flipForWritingModeLocalCoordinates(flippedRect);
        overflow->addVisualOverflow(flippedRect);
    }
}

LayoutRect RenderFragmentContainer::visualOverflowRectForBox(const RenderBox& box) const
{
    if (CheckedPtr overflow = overflowForBox(box))
        return overflow->visualOverflowRect();
    return computedVisualOverflowRectForBox(box);
}

// FIXME: This doesn't work for writing modes.
LayoutRect RenderFragmentContainer::layoutOverflowRectForBoxForPropagation(const RenderBox& box)
{
    // Only propagate interior layout overflow if we don't clip it.
    LayoutRect rect = box.borderBoxRect();
    rect = rectFlowPortionForBox(box, rect);
    if (!box.hasNonVisibleOverflow()) {
        auto layoutOverflowRect = LayoutRect { };
        if (CheckedPtr overflow = overflowForBox(box))
            layoutOverflowRect = overflow->layoutOverflowRect();
        else
            layoutOverflowRect = computedLayoutOverflowRectForBox(box);
        rect.unite(layoutOverflowRect);
    }

    bool hasTransform = box.isTransformed();
    if (box.isInFlowPositioned() || hasTransform) {
        if (hasTransform)
            rect = box.layer()->currentTransform().mapRect(rect);

        if (box.isInFlowPositioned())
            rect.move(box.offsetForInFlowPosition());
    }

    return rect;
}

LayoutRect RenderFragmentContainer::visualOverflowRectForBoxForPropagation(const RenderBox& box)
{
    LayoutRect rect = visualOverflowRectForBox(box);
    fragmentedFlow()->flipForWritingModeLocalCoordinates(rect);

    return rect;
}

CurrentRenderFragmentContainerMaintainer::CurrentRenderFragmentContainerMaintainer(RenderFragmentContainer& fragment)
    : m_fragment(fragment)
{
    RenderFragmentedFlow* fragmentedFlow = fragment.fragmentedFlow();
    // A flow thread can have only one current fragment.
    ASSERT(!fragmentedFlow->currentFragment());
    fragmentedFlow->setCurrentFragmentMaintainer(this);
}

CurrentRenderFragmentContainerMaintainer::~CurrentRenderFragmentContainerMaintainer()
{
    RenderFragmentedFlow* fragmentedFlow = m_fragment.fragmentedFlow();
    fragmentedFlow->setCurrentFragmentMaintainer(nullptr);
}

#ifndef NDEBUG

TextStream& operator<<(TextStream& stream, const RenderFragmentContainer& container)
{
    return stream << &container;
}

#endif

} // namespace WebCore
