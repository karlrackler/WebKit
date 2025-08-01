/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009-2016 Google, Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
 */

#pragma once

#include "FloatRect.h"
#include "RenderReplaced.h"
#include "SVGBoundingBoxComputation.h"

namespace WebCore {

class RenderSVGViewportContainer;
class SVGSVGElement;

class RenderSVGRoot final : public RenderReplaced {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGRoot);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGRoot);
public:
    RenderSVGRoot(SVGSVGElement&, RenderStyle&&);
    virtual ~RenderSVGRoot();

    SVGSVGElement& svgSVGElement() const;
    FloatSize computeViewportSize() const;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    std::pair<FloatSize, FloatSize> computeIntrinsicSizeAndPreferredAspectRatio() const final;
    bool hasIntrinsicAspectRatio() const final;

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didTransformToRootUpdate() const { return m_didTransformToRootUpdate; }
    bool isInLayout() const { return m_inLayout; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

    bool hasRelativeDimensions() const final;

    bool shouldApplyViewportClip() const;

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect objectBoundingBoxWithoutTransformations() const final { return m_objectBoundingBoxWithoutTransformations; }
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

    LayoutRect visualOverflowRectEquivalent() const { return SVGBoundingBoxComputation::computeVisualOverflowRect(*this); }

    RenderSVGViewportContainer* viewportContainer() const;
    CheckedPtr<RenderSVGViewportContainer> checkedViewportContainer() const;

private:
    void element() const = delete;

    ASCIILiteral renderName() const final { return "RenderSVGRoot"_s; }
    bool requiresLayer() const final { return true; }

    bool updateLayoutSizeIfNeeded();
    bool paintingAffectedByExternalOffset() const;

    // To prevent certain legacy code paths to hit assertions in debug builds, when switching off LBSE (during the teardown of the LBSE tree).
    std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject*, VisibleRectContext) const final { return std::nullopt; }

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ShouldComputePreferred::ComputeActual) const final;
    LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const final;
    void layout() final;
    void layoutChildren();
    void paint(PaintInfo&, const LayoutPoint&) final;
    void paintObject(PaintInfo&, const LayoutPoint&) final;
    void paintContents(PaintInfo&, const LayoutPoint&);

    void willBeDestroyed() final;

    void updateFromStyle() final;
    bool needsHasSVGTransformFlags() const final;
    void updateLayerTransform() final;

    FloatSize calculateIntrinsicSize() const;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarSizeRelevancy = OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const final;

    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const final;

    void boundingRects(Vector<LayoutRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;

    bool canBeSelectionLeaf() const final { return false; }
    bool canHaveChildren() const final { return true; }

    bool m_inLayout { false };
    bool m_didTransformToRootUpdate { false };
    bool m_isLayoutSizeChanged { false };

    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    FloatRect m_objectBoundingBoxWithoutTransformations;
    mutable Markable<FloatRect> m_strokeBoundingBox;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGRoot, isRenderSVGRoot())
