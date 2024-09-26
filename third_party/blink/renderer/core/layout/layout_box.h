/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_

#include <memory>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/overflow_model.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class CustomLayoutChild;
class LayoutBlockFlow;
class LayoutMultiColumnSpannerPlaceholder;
class NGBoxFragmentBuilder;
class NGBlockBreakToken;
class NGColumnSpannerPath;
class NGConstraintSpace;
class NGEarlyBreak;
class NGLayoutResult;
class ShapeOutsideInfo;
enum class NGLayoutCacheStatus;
struct BoxLayoutExtraInput;
struct NGFragmentGeometry;
struct NGPhysicalBoxStrut;
struct PaintInfo;

enum SizeType { kMainOrPreferredSize, kMinSize, kMaxSize };
enum AvailableLogicalHeightType {
  kExcludeMarginBorderPadding,
  kIncludeMarginBorderPadding
};
// When painting, overlay scrollbars do not take up space and should not affect
// clipping behavior. During hit testing, overlay scrollbars behave like regular
// scrollbars and should change how hit testing is clipped.
enum MarginDirection { kBlockDirection, kInlineDirection };

enum BackgroundRectType {
  kBackgroundPaintedExtent,
  kBackgroundKnownOpaqueRect,
};

enum ShouldComputePreferred { kComputeActual, kComputePreferred };

enum ShouldClampToContentBox { kDoNotClampToContentBox, kClampToContentBox };

enum ShouldIncludeScrollbarGutter {
  kExcludeScrollbarGutter,
  kIncludeScrollbarGutter
};

using SnapAreaSet = HeapHashSet<Member<LayoutBox>>;

struct LayoutBoxRareData final : public GarbageCollected<LayoutBoxRareData> {
 public:
  LayoutBoxRareData();
  LayoutBoxRareData(const LayoutBoxRareData&) = delete;
  LayoutBoxRareData& operator=(const LayoutBoxRareData&) = delete;

  void Trace(Visitor* visitor) const;

  // For spanners, the spanner placeholder that lays us out within the multicol
  // container.
  Member<LayoutMultiColumnSpannerPlaceholder> spanner_placeholder_;

  LayoutUnit override_logical_width_;
  LayoutUnit override_logical_height_;

  bool has_override_containing_block_content_logical_width_ : 1;
  bool has_override_containing_block_content_logical_height_ : 1;
  bool has_previous_content_box_rect_ : 1;

  LayoutUnit override_containing_block_content_logical_width_;
  LayoutUnit override_containing_block_content_logical_height_;

  // For snap area, the owning snap container.
  Member<LayoutBox> snap_container_;
  // For snap container, the descendant snap areas that contribute snap
  // points.
  SnapAreaSet snap_areas_;

  // Used by BoxPaintInvalidator. Stores the previous content rect after the
  // last paint invalidation. It's valid if has_previous_content_box_rect_ is
  // true.
  PhysicalRect previous_physical_content_box_rect_;

  // Used by CSSLayoutDefinition::Instance::Layout. Represents the script
  // object for this box that web developers can query style, and perform
  // layout upon. Only created if IsCustomItem() is true.
  Member<CustomLayoutChild> layout_child_;
};

// LayoutBox implements the full CSS box model.
//
// LayoutBoxModelObject only introduces some abstractions for LayoutInline and
// LayoutBox. The logic for the model is in LayoutBox, e.g. the storage for the
// rectangle and offset forming the CSS box (frame_location_ and frame_size_)
// and the getters for the different boxes.
//
// LayoutBox is also the uppermost class to support scrollbars, however the
// logic is delegated to PaintLayerScrollableArea.
// Per the CSS specification, scrollbars should "be inserted between the inner
// border edge and the outer padding edge".
// (see http://www.w3.org/TR/CSS21/visufx.html#overflow)
// Also the scrollbar width / height are removed from the content box. Taking
// the following example:
//
// <!DOCTYPE html>
// <style>
// ::-webkit-scrollbar {
//     /* Force non-overlay scrollbars */
//     width: 10px;
//     height: 20px;
// }
// </style>
// <div style="overflow:scroll; width: 100px; height: 100px">
//
// The <div>'s content box is not 100x100 as specified in the style but 90x80 as
// we remove the scrollbars from the box.
//
// The presence of scrollbars is determined by the 'overflow' property and can
// be conditioned on having layout overflow (see OverflowModel for more details
// on how we track overflow).
//
// There are 2 types of scrollbars:
// - non-overlay scrollbars take space from the content box.
// - overlay scrollbars don't and just overlay hang off from the border box,
//   potentially overlapping with the padding box's content.
// For more details on scrollbars, see PaintLayerScrollableArea.
//
//
// ***** THE BOX MODEL *****
// The CSS box model is based on a series of nested boxes:
// http://www.w3.org/TR/CSS21/box.html
//
//       |----------------------------------------------------|
//       |                                                    |
//       |                   margin-top                       |
//       |                                                    |
//       |     |-----------------------------------------|    |
//       |     |                                         |    |
//       |     |             border-top                  |    |
//       |     |                                         |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                          |    |    |    |
//       |     |    |       padding-top        |####|    |    |
//       |     |    |                          |####|    |    |
//       |     |    |    |----------------|    |####|    |    |
//       |     |    |    |                |    |    |    |    |
//       | ML  | BL | PL |  content box   | PR | SW | BR | MR |
//       |     |    |    |                |    |    |    |    |
//       |     |    |    |----------------|    |    |    |    |
//       |     |    |                          |    |    |    |
//       |     |    |      padding-bottom      |    |    |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |     scrollbar height ####| SC |    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |-------------------------------|    |    |
//       |     |                                         |    |
//       |     |           border-bottom                 |    |
//       |     |                                         |    |
//       |     |-----------------------------------------|    |
//       |                                                    |
//       |                 margin-bottom                      |
//       |                                                    |
//       |----------------------------------------------------|
//
// BL = border-left
// BR = border-right
// ML = margin-left
// MR = margin-right
// PL = padding-left
// PR = padding-right
// SC = scroll corner (contains UI for resizing (see the 'resize' property)
// SW = scrollbar width
//
// Note that the vertical scrollbar (if existing) will be on the left in
// right-to-left direction and horizontal writing-mode. The horizontal scrollbar
// (if existing) is always at the bottom.
//
// Those are just the boxes from the CSS model. Extra boxes are tracked by Blink
// (e.g. the overflows). Thus it is paramount to know which box a function is
// manipulating. Also of critical importance is the coordinate system used (see
// the COORDINATE SYSTEMS section in LayoutBoxModelObject).
class CORE_EXPORT LayoutBox : public LayoutBoxModelObject {
 public:
  explicit LayoutBox(ContainerNode*);
  void Trace(Visitor*) const override;

  PaintLayerType LayerTypeRequired() const override;

  bool BackgroundIsKnownToBeOpaqueInRect(
      const PhysicalRect& local_rect) const override;
  bool TextIsKnownToBeOnOpaqueBackground() const override;

  virtual bool BackgroundShouldAlwaysBeClipped() const {
    NOT_DESTROYED();
    return false;
  }

  // Returns whether this object needs a scroll paint property tree node. These
  // are a requirement for composited scrolling but are also created for
  // non-composited scrollers.
  bool NeedsScrollNode(CompositingReasons direct_compositing_reasons) const;

  // Use this with caution! No type checking is done!
  LayoutBox* FirstChildBox() const;
  LayoutBox* FirstInFlowChildBox() const;
  LayoutBox* LastChildBox() const;

  void SetWidth(LayoutUnit width) {
    NOT_DESTROYED();
    if (width == frame_size_.Width()) {
      return;
    }
    frame_size_.SetWidth(width);
    SizeChanged();
  }
  void SetHeight(LayoutUnit height) {
    NOT_DESTROYED();
    if (height == frame_size_.Height()) {
      return;
    }
    frame_size_.SetHeight(height);
    SizeChanged();
  }

  LayoutUnit LogicalLeft() const {
    NOT_DESTROYED();
    auto location = Location();
    return StyleRef().IsHorizontalWritingMode() ? location.X() : location.Y();
  }
  LayoutUnit LogicalRight() const {
    NOT_DESTROYED();
    return LogicalLeft() + LogicalWidth();
  }
  LayoutUnit LogicalTop() const {
    NOT_DESTROYED();
    auto location = Location();
    return StyleRef().IsHorizontalWritingMode() ? location.Y() : location.X();
  }
  LayoutUnit LogicalBottom() const {
    NOT_DESTROYED();
    return LogicalTop() + LogicalHeight();
  }
  LayoutUnit LogicalWidth() const {
    NOT_DESTROYED();
    LayoutSize size = Size();
    return StyleRef().IsHorizontalWritingMode() ? size.Width() : size.Height();
  }
  LayoutUnit LogicalHeight() const {
    NOT_DESTROYED();
    LayoutSize size = Size();
    return StyleRef().IsHorizontalWritingMode() ? size.Height() : size.Width();
  }

  // Logical height of the object, including content overflowing the
  // border-after edge.
  virtual LayoutUnit LogicalHeightWithVisibleOverflow() const;

  LayoutUnit ConstrainLogicalHeightByMinMax(
      LayoutUnit logical_height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ConstrainContentBoxLogicalHeightByMinMax(
      LayoutUnit logical_height,
      LayoutUnit intrinsic_content_height) const;

  LayoutUnit LogicalHeightForEmptyLine() const {
    NOT_DESTROYED();
    return FirstLineHeight();
  }

  void SetLogicalWidth(LayoutUnit size) {
    NOT_DESTROYED();
    DCHECK(!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled());
    if (StyleRef().IsHorizontalWritingMode())
      SetWidth(size);
    else
      SetHeight(size);
  }
  void SetLogicalHeight(LayoutUnit size) {
    NOT_DESTROYED();
    DCHECK(!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled());
    if (StyleRef().IsHorizontalWritingMode())
      SetHeight(size);
    else
      SetWidth(size);
  }

  virtual LayoutPoint Location() const {
    NOT_DESTROYED();
    return frame_location_;
  }
  LayoutSize LocationOffset() const {
    NOT_DESTROYED();
    auto location = Location();
    return LayoutSize(location.X(), location.Y());
  }
  virtual LayoutSize Size() const;

  void SetLocation(const LayoutPoint& location) {
    NOT_DESTROYED();
    if (location == frame_location_) {
      return;
    }
    frame_location_ = location;
    LocationChanged();
  }

  // The ancestor box that this object's Location and PhysicalLocation are
  // relative to.
  virtual LayoutBox* LocationContainer() const;

  // FIXME: Currently scrollbars are using int geometry and positioned based on
  // pixelSnappedBorderBoxRect whose size may change when location changes
  // because of pixel snapping. This function is used to change location of the
  // LayoutBox outside of LayoutBox::layout(). Will remove when we use
  // LayoutUnits for scrollbars.
  void SetLocationAndUpdateOverflowControlsIfNeeded(const LayoutPoint&);

  void SetSize(const LayoutSize& size) {
    NOT_DESTROYED();
    DCHECK(!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled());
    if (size == frame_size_) {
      return;
    }
    frame_size_ = size;
    SizeChanged();
  }

  // See frame_location_ and frame_size_.
  LayoutRect FrameRect() const {
    NOT_DESTROYED();
    return LayoutRect(Location(), Size());
  }

  // Note that those functions have their origin at this box's CSS border box.
  // As such their location doesn't account for 'top'/'left'. About its
  // coordinate space, it can be treated as in either physical coordinates
  // or "physical coordinates in flipped block-flow direction", and
  // FlipForWritingMode() will do nothing on it.
  LayoutRect BorderBoxRect() const {
    NOT_DESTROYED();
    return LayoutRect(LayoutPoint(), Size());
  }
  PhysicalRect PhysicalBorderBoxRect() const {
    NOT_DESTROYED();
    // This doesn't need flipping because the result would be the same.
    DCHECK_EQ(PhysicalRect(BorderBoxRect()),
              FlipForWritingMode(BorderBoxRect()));
    return PhysicalRect(BorderBoxRect());
  }

  // Client rect and padding box rect are the same concept.
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  DISABLE_CFI_PERF PhysicalRect PhysicalPaddingBoxRect() const {
    NOT_DESTROYED();
    return PhysicalRect(ClientLeft(), ClientTop(), ClientWidth(),
                        ClientHeight());
  }

  // TODO(crbug.com/962299): This method snaps to pixels incorrectly because
  // Location() is not the correct paint offset. It's also incorrect in flipped
  // blocks writing mode.
  gfx::Rect PixelSnappedBorderBoxRect() const {
    NOT_DESTROYED();
    return gfx::Rect(PixelSnappedBorderBoxSize(PhysicalOffset(Location())));
  }
  // TODO(crbug.com/962299): This method is only correct when |offset| is the
  // correct paint offset.
  gfx::Size PixelSnappedBorderBoxSize(const PhysicalOffset& offset) const {
    NOT_DESTROYED();
    return ToPixelSnappedSize(Size(), offset.ToLayoutPoint());
  }
  gfx::Rect BorderBoundingBox() const final {
    NOT_DESTROYED();
    return PixelSnappedBorderBoxRect();
  }

  // The content area of the box (excludes padding - and intrinsic padding for
  // table cells, etc... - and scrollbars and border).
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  DISABLE_CFI_PERF PhysicalRect PhysicalContentBoxRect() const {
    NOT_DESTROYED();
    return PhysicalRect(ContentLeft(), ContentTop(), ContentWidth(),
                        ContentHeight());
  }
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  PhysicalOffset PhysicalContentBoxOffset() const {
    NOT_DESTROYED();
    return PhysicalOffset(ContentLeft(), ContentTop());
  }
  PhysicalSize PhysicalContentBoxSize() const {
    NOT_DESTROYED();
    return PhysicalSize(ContentWidth(), ContentHeight());
  }
  // The content box converted to absolute coords (taking transforms into
  // account).
  gfx::QuadF AbsoluteContentQuad(MapCoordinatesFlags = 0) const;

  // The enclosing rectangle of the background with given opacity requirement.
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  PhysicalRect PhysicalBackgroundRect(BackgroundRectType) const;

  // This returns the content area of the box (excluding padding and border).
  // The only difference with contentBoxRect is that computedCSSContentBoxRect
  // does include the intrinsic padding in the content box as this is what some
  // callers expect (like getComputedStyle).
  LayoutRect ComputedCSSContentBoxRect() const {
    NOT_DESTROYED();
    return LayoutRect(
        BorderLeft() + ComputedCSSPaddingLeft(),
        BorderTop() + ComputedCSSPaddingTop(),
        ClientWidth() - ComputedCSSPaddingLeft() - ComputedCSSPaddingRight(),
        ClientHeight() - ComputedCSSPaddingTop() - ComputedCSSPaddingBottom());
  }

  void AddOutlineRects(OutlineRectCollector&,
                       OutlineInfo*,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const override;

  // Use this with caution! No type checking is done!
  LayoutBox* PreviousSiblingBox() const;
  LayoutBox* PreviousInFlowSiblingBox() const;
  LayoutBox* NextSiblingBox() const;
  LayoutBox* NextInFlowSiblingBox() const;
  LayoutBox* ParentBox() const;

  // Return the previous sibling column set or spanner placeholder. Only to be
  // used on multicol container children.
  LayoutBox* PreviousSiblingMultiColumnBox() const;
  // Return the next sibling column set or spanner placeholder. Only to be used
  // on multicol container children.
  LayoutBox* NextSiblingMultiColumnBox() const;

  bool CanResize() const;

  bool ShouldComputeLogicalHeightFromAspectRatio() const {
    NOT_DESTROYED();
    if (ShouldComputeLogicalWidthFromAspectRatioAndInsets())
      return false;
    Length h = StyleRef().LogicalHeight();
    return !StyleRef().AspectRatio().IsAuto() &&
           (h.IsAuto() || h.IsMinContent() || h.IsMaxContent() ||
            h.IsFitContent() ||
            (!IsOutOfFlowPositioned() && h.IsPercentOrCalc() &&
             ComputePercentageLogicalHeight(h) == kIndefiniteSize));
  }
  bool ShouldComputeLogicalWidthFromAspectRatioAndInsets() const {
    NOT_DESTROYED();
    const ComputedStyle& style = StyleRef();
    if (style.AspectRatio().IsAuto() || !IsOutOfFlowPositioned())
      return false;
    if (style.UsedWidth().IsAuto() && style.UsedHeight().IsAuto() &&
        !style.LogicalTop().IsAuto() && !style.LogicalBottom().IsAuto() &&
        (style.LogicalLeft().IsAuto() || style.LogicalRight().IsAuto())) {
      return true;
    }
    return false;
  }

  MinMaxSizes ComputeMinMaxLogicalWidthFromAspectRatio() const;

  // Like most of the other box geometries, visual and layout overflow are also
  // in the "physical coordinates in flipped block-flow direction" of the box.
  LayoutRect NoOverflowRect() const;
  LayoutRect LayoutOverflowRect() const {
    NOT_DESTROYED();
    DCHECK(!IsLayoutMultiColumnSet());
    return LayoutOverflowIsSet()
               ? overflow_->layout_overflow->LayoutOverflowRect()
               : NoOverflowRect();
  }
  PhysicalRect PhysicalLayoutOverflowRect() const {
    NOT_DESTROYED();
    return FlipForWritingMode(LayoutOverflowRect());
  }
  LayoutSize MaxLayoutOverflow() const {
    NOT_DESTROYED();
    return LayoutSize(LayoutOverflowRect().MaxX(), LayoutOverflowRect().MaxY());
  }

  LayoutRect VisualOverflowRect() const;
  PhysicalRect PhysicalVisualOverflowRect() const final {
    NOT_DESTROYED();
    return FlipForWritingMode(VisualOverflowRect());
  }
  // VisualOverflow has DCHECK for reading before it is computed. These
  // functions pretend there is no visual overflow when it is not computed.
  // TODO(crbug.com/1205708): Audit the usages and fix issues.
#if DCHECK_IS_ON()
  LayoutRect VisualOverflowRectAllowingUnset() const;
  PhysicalRect PhysicalVisualOverflowRectAllowingUnset() const;
#else
  ALWAYS_INLINE LayoutRect VisualOverflowRectAllowingUnset() const {
    NOT_DESTROYED();
    return VisualOverflowRect();
  }
  ALWAYS_INLINE PhysicalRect PhysicalVisualOverflowRectAllowingUnset() const {
    NOT_DESTROYED();
    return PhysicalVisualOverflowRect();
  }
#endif
  LayoutUnit LogicalLeftVisualOverflow() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? VisualOverflowRect().X()
                                                : VisualOverflowRect().Y();
  }
  LayoutUnit LogicalRightVisualOverflow() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? VisualOverflowRect().MaxX()
                                                : VisualOverflowRect().MaxY();
  }

  LayoutRect SelfVisualOverflowRect() const {
    NOT_DESTROYED();
    return VisualOverflowIsSet()
               ? overflow_->visual_overflow->SelfVisualOverflowRect()
               : BorderBoxRect();
  }
  PhysicalRect PhysicalSelfVisualOverflowRect() const {
    NOT_DESTROYED();
    return FlipForWritingMode(SelfVisualOverflowRect());
  }
  LayoutRect ContentsVisualOverflowRect() const {
    NOT_DESTROYED();
    return VisualOverflowIsSet()
               ? overflow_->visual_overflow->ContentsVisualOverflowRect()
               : LayoutRect();
  }
  PhysicalRect PhysicalContentsVisualOverflowRect() const {
    NOT_DESTROYED();
    return FlipForWritingMode(ContentsVisualOverflowRect());
  }

  // These methods don't mean the box *actually* has top/left overflow. They
  // mean that *if* the box overflows, it will overflow to the top/left rather
  // than the bottom/right. This happens when child content is laid out
  // right-to-left (e.g. direction:rtl) or or bottom-to-top (e.g. direction:rtl
  // writing-mode:vertical-rl).
  virtual bool HasTopOverflow() const;
  virtual bool HasLeftOverflow() const;

  // Sets the layout-overflow from the current set of layout-results.
  void SetLayoutOverflowFromLayoutResults();

  void AddLayoutOverflow(const LayoutRect&);
  void AddSelfVisualOverflow(const PhysicalRect& r) {
    NOT_DESTROYED();
    AddSelfVisualOverflow(FlipForWritingMode(r));
  }
  void AddSelfVisualOverflow(const LayoutRect&);
  void AddContentsVisualOverflow(const PhysicalRect& r) {
    NOT_DESTROYED();
    AddContentsVisualOverflow(FlipForWritingMode(r));
  }
  void AddContentsVisualOverflow(const LayoutRect&);

  virtual void AddVisualEffectOverflow();
  NGPhysicalBoxStrut ComputeVisualEffectOverflowOutsets();
  void AddVisualOverflowFromChild(const LayoutBox& child) {
    NOT_DESTROYED();
    AddVisualOverflowFromChild(child, child.LocationOffset());
  }
  void AddVisualOverflowFromChild(const LayoutBox& child,
                                  const LayoutSize& delta);

  void ClearLayoutOverflow();
  void ClearVisualOverflow();

  bool CanUseFragmentsForVisualOverflow() const;
  void RecalcFragmentsVisualOverflow();
  void CopyVisualOverflowFromFragments();

  virtual void UpdateAfterLayout();

  DISABLE_CFI_PERF LayoutUnit ContentLeft() const {
    NOT_DESTROYED();
    return ClientLeft() + PaddingLeft();
  }
  DISABLE_CFI_PERF LayoutUnit ContentTop() const {
    NOT_DESTROYED();
    return ClientTop() + PaddingTop();
  }
  DISABLE_CFI_PERF LayoutUnit ContentWidth() const {
    NOT_DESTROYED();
    // We're dealing with LayoutUnit and saturated arithmetic here, so we need
    // to guard against negative results. The value returned from clientWidth()
    // may in itself be a victim of saturated arithmetic; e.g. if both border
    // sides were sufficiently wide (close to LayoutUnit::max()).  Here we
    // subtract two padding values from that result, which is another source of
    // saturated arithmetic.
    return (ClientWidth() - PaddingLeft() - PaddingRight())
        .ClampNegativeToZero();
  }
  DISABLE_CFI_PERF LayoutUnit ContentHeight() const {
    NOT_DESTROYED();
    // We're dealing with LayoutUnit and saturated arithmetic here, so we need
    // to guard against negative results. The value returned from clientHeight()
    // may in itself be a victim of saturated arithmetic; e.g. if both border
    // sides were sufficiently wide (close to LayoutUnit::max()).  Here we
    // subtract two padding values from that result, which is another source of
    // saturated arithmetic.
    return (ClientHeight() - PaddingTop() - PaddingBottom())
        .ClampNegativeToZero();
  }
  LayoutSize ContentSize() const {
    NOT_DESTROYED();
    return LayoutSize(ContentWidth(), ContentHeight());
  }
  LayoutUnit ContentLogicalWidth() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? ContentWidth()
                                                : ContentHeight();
  }
  LayoutUnit ContentLogicalHeight() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? ContentHeight()
                                                : ContentWidth();
  }

  bool ShouldUseAutoIntrinsicSize() const;
  // CSS intrinsic sizing getters.
  // https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
  // Physical:
  bool HasOverrideIntrinsicContentWidth() const;
  bool HasOverrideIntrinsicContentHeight() const;
  LayoutUnit OverrideIntrinsicContentWidth() const;
  LayoutUnit OverrideIntrinsicContentHeight() const;
  // Logical:
  bool HasOverrideIntrinsicContentLogicalWidth() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode()
               ? HasOverrideIntrinsicContentWidth()
               : HasOverrideIntrinsicContentHeight();
  }
  bool HasOverrideIntrinsicContentLogicalHeight() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode()
               ? HasOverrideIntrinsicContentHeight()
               : HasOverrideIntrinsicContentWidth();
  }
  LayoutUnit OverrideIntrinsicContentLogicalWidth() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode()
               ? OverrideIntrinsicContentWidth()
               : OverrideIntrinsicContentHeight();
  }
  LayoutUnit OverrideIntrinsicContentLogicalHeight() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode()
               ? OverrideIntrinsicContentHeight()
               : OverrideIntrinsicContentWidth();
  }

  // Returns element-native intrinsic size. Returns kIndefiniteSize if no such
  // size.
  LayoutUnit DefaultIntrinsicContentInlineSize() const;
  LayoutUnit DefaultIntrinsicContentBlockSize() const;

  // IE extensions. Used to calculate offsetWidth/Height. Overridden by inlines
  // (LayoutFlow) to return the remaining width on a given line (and the height
  // of a single line).
  LayoutUnit OffsetWidth() const final {
    NOT_DESTROYED();
    return Size().Width();
  }
  LayoutUnit OffsetHeight() const final {
    NOT_DESTROYED();
    return Size().Height();
  }

  bool UsesOverlayScrollbars() const;

  // Clamps the left scrollbar size so it is not wider than the content box.
  DISABLE_CFI_PERF LayoutUnit LogicalLeftScrollbarWidth() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars())
      return LayoutUnit();
    else if (StyleRef().IsHorizontalWritingMode())
      return ComputeScrollbarsInternal(kClampToContentBox).left;
    else
      return ComputeScrollbarsInternal(kClampToContentBox).top;
  }
  DISABLE_CFI_PERF LayoutUnit LogicalTopScrollbarHeight() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars())
      return LayoutUnit();
    else if (HasFlippedBlocksWritingMode())
      return ComputeScrollbarsInternal(kClampToContentBox).right;
    else
      return ComputeScrollbarsInternal(kClampToContentBox).top;
  }

  // Physical client rect (a.k.a. PhysicalPaddingBoxRect(), defined by
  // ClientLeft, ClientTop, ClientWidth and ClientHeight) represents the
  // interior of an object excluding borders and scrollbars.
  // Clamps the left scrollbar size so it is not wider than the content box.
  DISABLE_CFI_PERF LayoutUnit ClientLeft() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars())
      return BorderLeft();
    else
      return BorderLeft() + ComputeScrollbarsInternal(kClampToContentBox).left;
  }
  DISABLE_CFI_PERF LayoutUnit ClientTop() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars())
      return BorderTop();
    else
      return BorderTop() + ComputeScrollbarsInternal(kClampToContentBox).top;
  }

  // Size without borders and scrollbars.
  LayoutUnit ClientWidth() const;
  LayoutUnit ClientHeight() const;
  // Similar to ClientWidth() and ClientHeight(), but based on the specified
  // border-box size.
  LayoutUnit ClientWidthFrom(LayoutUnit width) const;
  LayoutUnit ClientHeightFrom(LayoutUnit height) const;
  DISABLE_CFI_PERF LayoutUnit ClientLogicalWidth() const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? ClientWidth() : ClientHeight();
  }
  DISABLE_CFI_PERF LayoutUnit ClientLogicalHeight() const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? ClientHeight() : ClientWidth();
  }
  DISABLE_CFI_PERF LayoutUnit ClientLogicalBottom() const {
    NOT_DESTROYED();
    return BorderBefore() + LogicalTopScrollbarHeight() + ClientLogicalHeight();
  }

  // TODO(crbug.com/962299): This is incorrect in some cases.
  int PixelSnappedClientWidth() const;
  int PixelSnappedClientHeight() const;

  LayoutUnit ClientWidthWithTableSpecialBehavior() const;
  LayoutUnit ClientHeightWithTableSpecialBehavior() const;

  // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight
  // unless the object has overflow:hidden/scroll/auto specified and also has
  // overflow. These methods are virtual so that objects like textareas can
  // scroll shadow content (but pretend that they are the objects that are
  // scrolling).

  // Replaced ScrollLeft/Top by using Element::GetLayoutBoxForScrolling to
  // return the correct ScrollableArea.
  // TODO(cathiechen): We should do the same with ScrollWidth|Height .
  virtual LayoutUnit ScrollWidth() const;
  virtual LayoutUnit ScrollHeight() const;
  // TODO(crbug.com/962299): This is incorrect in some cases.
  int PixelSnappedScrollWidth() const;
  int PixelSnappedScrollHeight() const;

  NGPhysicalBoxStrut MarginBoxOutsets() const {
    NOT_DESTROYED();
    return margin_box_outsets_;
  }
  LayoutUnit MarginTop() const override {
    NOT_DESTROYED();
    return margin_box_outsets_.top;
  }
  LayoutUnit MarginBottom() const override {
    NOT_DESTROYED();
    return margin_box_outsets_.bottom;
  }
  LayoutUnit MarginLeft() const override {
    NOT_DESTROYED();
    return margin_box_outsets_.left;
  }
  LayoutUnit MarginRight() const override {
    NOT_DESTROYED();
    return margin_box_outsets_.right;
  }
  void SetMargin(const NGPhysicalBoxStrut&);
  void SetMarginBefore(LayoutUnit value, const ComputedStyle& override_style) {
    NOT_DESTROYED();
    WritingDirectionMode mode = override_style.GetWritingDirection();
    NGBoxStrut logical_margin = margin_box_outsets_.ConvertToLogical(mode);
    logical_margin.block_start = value;
    margin_box_outsets_ = logical_margin.ConvertToPhysical(mode);
  }
  void SetMarginAfter(LayoutUnit value, const ComputedStyle& override_style) {
    NOT_DESTROYED();
    WritingDirectionMode mode = override_style.GetWritingDirection();
    NGBoxStrut logical_margin = margin_box_outsets_.ConvertToLogical(mode);
    logical_margin.block_end = value;
    margin_box_outsets_ = logical_margin.ConvertToPhysical(mode);
  }

  void AbsoluteQuads(Vector<gfx::QuadF>&,
                     MapCoordinatesFlags mode = 0) const override;
  gfx::RectF LocalBoundingBoxRectForAccessibility() const override;

  void SetBoxLayoutExtraInput(const BoxLayoutExtraInput* input) {
    NOT_DESTROYED();
    extra_input_ = input;
  }
  const BoxLayoutExtraInput* GetBoxLayoutExtraInput() const {
    NOT_DESTROYED();
    return extra_input_;
  }

  void LayoutSubtreeRoot();
  void LayoutSubtreeRootOld();

  void UpdateLayout() override;
  void Paint(const PaintInfo&) const override;

  virtual bool IsInSelfHitTestingPhase(HitTestPhase phase) const {
    NOT_DESTROYED();
    return phase == HitTestPhase::kForeground;
  }

  bool HitTestAllPhases(HitTestResult&,
                        const HitTestLocation&,
                        const PhysicalOffset& accumulated_offset) final;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase) override;
  // Fast check if |NodeAtPoint| may find a hit.
  bool MayIntersect(const HitTestResult& result,
                    const HitTestLocation& hit_test_location,
                    const PhysicalOffset& accumulated_offset) const;

  // This function calculates the preferred widths for an object.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS in layout_object.h for more
  // details about those widths.
  MinMaxSizes PreferredLogicalWidths() const override;

  LayoutUnit OverrideLogicalHeight() const;
  LayoutUnit OverrideLogicalWidth() const;
  bool IsOverrideLogicalHeightDefinite() const;
  bool StretchInlineSizeIfAuto() const;
  bool StretchBlockSizeIfAuto() const;
  bool HasOverrideLogicalHeight() const;
  bool HasOverrideLogicalWidth() const;
  void SetOverrideLogicalHeight(LayoutUnit);
  void SetOverrideLogicalWidth(LayoutUnit);
  void ClearOverrideLogicalHeight();
  void ClearOverrideLogicalWidth();
  void ClearOverrideSize();

  LayoutUnit OverrideContentLogicalWidth() const;
  LayoutUnit OverrideContentLogicalHeight() const;

  LayoutUnit OverrideContainingBlockContentLogicalWidth() const;
  LayoutUnit OverrideContainingBlockContentLogicalHeight() const;
  bool HasOverrideContainingBlockContentLogicalWidth() const;
  bool HasOverrideContainingBlockContentLogicalHeight() const;
  void SetOverrideContainingBlockContentLogicalWidth(LayoutUnit);
  void SetOverrideContainingBlockContentLogicalHeight(LayoutUnit);
  void ClearOverrideContainingBlockContentSize();

  // When an available inline size override has been set, we'll use that to fill
  // available inline size, rather than deducing it from the containing block
  // (and then subtract space taken up by adjacent floats).
  LayoutUnit OverrideAvailableInlineSize() const;
  bool HasOverrideAvailableInlineSize() const {
    NOT_DESTROYED();
    return extra_input_;
  }

  LayoutUnit AdjustBorderBoxLogicalWidthForBoxSizing(float width) const;
  LayoutUnit AdjustBorderBoxLogicalHeightForBoxSizing(float height) const;
  LayoutUnit AdjustContentBoxLogicalWidthForBoxSizing(float width) const;
  LayoutUnit AdjustContentBoxLogicalHeightForBoxSizing(float height) const;

  // ComputedMarginValues holds the actual values for margins. It ignores
  // margin collapsing as they are handled in LayoutBlockFlow.
  // The margins are stored in logical coordinates (see COORDINATE
  // SYSTEMS in LayoutBoxModel) for use during layout.
  struct ComputedMarginValues {
    DISALLOW_NEW();
    ComputedMarginValues() = default;

    LayoutUnit before_;
    LayoutUnit after_;
    LayoutUnit start_;
    LayoutUnit end_;
  };

  // LogicalExtentComputedValues is used both for the
  // block-flow and inline-direction axis.
  struct LogicalExtentComputedValues {
    STACK_ALLOCATED();

   public:
    LogicalExtentComputedValues() = default;

    void CopyExceptBlockMargins(LogicalExtentComputedValues* out) const {
      out->extent_ = extent_;
      out->position_ = position_;
      out->margins_.start_ = margins_.start_;
      out->margins_.end_ = margins_.end_;
    }

    // This is the dimension in the measured direction
    // (logical height or logical width).
    LayoutUnit extent_;

    // This is the offset in the measured direction
    // (logical top or logical left).
    LayoutUnit position_;

    // |margins_| represents the margins in the measured direction.
    // Note that ComputedMarginValues has also the margins in
    // the orthogonal direction to have clearer names but they are
    // ignored in the code.
    ComputedMarginValues margins_;
  };

  // Resolve auto margins in the chosen direction of the containing block so
  // that objects can be pushed to the start, middle or end of the containing
  // block.
  void ComputeMarginsForDirection(MarginDirection for_direction,
                                  const LayoutBlock* containing_block,
                                  LayoutUnit container_width,
                                  LayoutUnit child_width,
                                  LayoutUnit& margin_start,
                                  LayoutUnit& margin_end,
                                  Length margin_start_length,
                                  Length margin_start_end) const;

  // Used to resolve margins in the containing block's block-flow direction.
  void ComputeAndSetBlockDirectionMargins(const LayoutBlock* containing_block);

  enum PageBoundaryRule { kAssociateWithFormerPage, kAssociateWithLatterPage };

  bool HasInlineFragments() const final;
  wtf_size_t FirstInlineFragmentItemIndex() const final;
  void ClearFirstInlineFragmentItemIndex() final;
  void SetFirstInlineFragmentItemIndex(wtf_size_t) final;

  void InvalidateItems(const NGLayoutResult&);

  void SetCachedLayoutResult(const NGLayoutResult*, wtf_size_t index);

  // Store one layout result (with its physical fragment) at the specified
  // index.
  //
  // If there's already a result at the specified index, use
  // ReplaceLayoutResult() to do the job. Otherwise, use AppendLayoutResult().
  //
  // If it's going to be the last result, we'll also perform any necessary
  // finalization (see FinalizeLayoutResults()), and also delete all the old
  // entries following it (if there used to be more results in a previous
  // layout).
  //
  // In a few specific cases we'll even delete the entries following this
  // result, even if it's *not* going to be the last one. This is necessary when
  // we might read out the layout results again before we've got to the end (OOF
  // block fragmentation, etc.). In all other cases, we'll leave the old results
  // until we're done, as deleting entries will trigger unnecessary paint
  // invalidation. With any luck, we'll end up with the same number of results
  // as the last time, so that paint invalidation might not be necessary.
  void SetLayoutResult(const NGLayoutResult*, wtf_size_t index);

  // Append one layout result at the end.
  void AppendLayoutResult(const NGLayoutResult*);

  // Replace a specific layout result. Also perform finalization if it's the
  // last result (see FinalizeLayoutResults()), but this function does not
  // delete any (old) results following this one. Callers should generally use
  // SetLayoutResult() instead of this one, unless they have good reasons not
  // to.
  void ReplaceLayoutResult(const NGLayoutResult*, wtf_size_t index);

  void ShrinkLayoutResults(wtf_size_t results_to_keep);

  // Perform any finalization needed after all the layout results have been
  // added.
  void FinalizeLayoutResults();

  void ClearLayoutResults();

  void RebuildFragmentTreeSpine();

  // Call when NG fragment count or size changed. Only call if the fragment
  // count is or was larger than 1.
  void FragmentCountOrSizeDidChange() {
    NOT_DESTROYED();
    // The fragment count may change, even if the total block-size remains the
    // same (if the fragmentainer block-size has changed, for instance).
    SetShouldDoFullPaintInvalidation();
  }

  const NGLayoutResult* GetCachedLayoutResult(const NGBlockBreakToken*) const;
  const NGLayoutResult* GetCachedMeasureResult() const;

  // Call in situations where we know that there's at most one fragment. A
  // DCHECK will fail if there are multiple fragments.
  const NGLayoutResult* GetSingleCachedLayoutResult() const;

  // Returns the last layout result for this block flow with the given
  // constraint space and break token, or null if it is not up-to-date or
  // otherwise unavailable.
  //
  // This method (while determining if the layout result can be reused), *may*
  // calculate the |initial_fragment_geometry| of the node.
  //
  // |out_cache_status| indicates what type of layout pass is required.
  //
  // TODO(ikilpatrick): Move this function into NGBlockNode.
  const NGLayoutResult* CachedLayoutResult(
      const NGConstraintSpace&,
      const NGBlockBreakToken*,
      const NGEarlyBreak*,
      const NGColumnSpannerPath*,
      absl::optional<NGFragmentGeometry>* initial_fragment_geometry,
      NGLayoutCacheStatus* out_cache_status);

  using NGLayoutResultList = HeapVector<Member<const NGLayoutResult>, 1>;
  class NGPhysicalFragmentList {
    STACK_ALLOCATED();

   public:
    explicit NGPhysicalFragmentList(const NGLayoutResultList& layout_results)
        : layout_results_(layout_results) {}

    wtf_size_t Size() const { return layout_results_.size(); }
    bool IsEmpty() const { return layout_results_.empty(); }

    bool HasFragmentItems() const;

    wtf_size_t IndexOf(const NGPhysicalBoxFragment& fragment) const;
    bool Contains(const NGPhysicalBoxFragment& fragment) const;

    class CORE_EXPORT Iterator {
     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = NGPhysicalBoxFragment;
      using difference_type = std::ptrdiff_t;
      using pointer = NGPhysicalBoxFragment*;
      using reference = NGPhysicalBoxFragment&;

      explicit Iterator(const NGLayoutResultList::const_iterator& iterator)
          : iterator_(iterator) {}

      const NGPhysicalBoxFragment& operator*() const;

      Iterator& operator++() {
        ++iterator_;
        return *this;
      }
      Iterator operator++(int) {
        Iterator copy = *this;
        ++*this;
        return copy;
      }

      bool operator==(const Iterator& other) const {
        return iterator_ == other.iterator_;
      }
      bool operator!=(const Iterator& other) const {
        return !operator==(other);
      }

     private:
      NGLayoutResultList::const_iterator iterator_;
    };

    Iterator begin() const { return Iterator(layout_results_.begin()); }
    Iterator end() const { return Iterator(layout_results_.end()); }

    const NGPhysicalBoxFragment& front() const;
    const NGPhysicalBoxFragment& back() const;

   private:
    const NGLayoutResultList& layout_results_;
  };

  NGPhysicalFragmentList PhysicalFragments() const {
    NOT_DESTROYED();
    return NGPhysicalFragmentList(layout_results_);
  }
  const NGLayoutResult* GetLayoutResult(wtf_size_t i) const;
  const NGLayoutResultList& GetLayoutResults() const {
    NOT_DESTROYED();
    return layout_results_;
  }
  const NGPhysicalBoxFragment* GetPhysicalFragment(wtf_size_t i) const;
  const FragmentData* FragmentDataFromPhysicalFragment(
      const NGPhysicalBoxFragment&) const;
  wtf_size_t PhysicalFragmentCount() const {
    NOT_DESTROYED();
    return layout_results_.size();
  }

  void SetSpannerPlaceholder(LayoutMultiColumnSpannerPlaceholder&);
  void ClearSpannerPlaceholder();
  LayoutMultiColumnSpannerPlaceholder* SpannerPlaceholder() const final {
    NOT_DESTROYED();
    return rare_data_ ? rare_data_->spanner_placeholder_ : nullptr;
  }

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  LayoutUnit ContainingBlockLogicalHeightForGetComputedStyle() const;

  LayoutUnit ContainingBlockLogicalWidthForContent() const override;
  LayoutUnit ContainingBlockLogicalHeightForContent(
      AvailableLogicalHeightType) const;

  LayoutUnit PerpendicularContainingBlockLogicalHeight() const;

  virtual void ComputeLogicalHeight(LayoutUnit logical_height,
                                    LayoutUnit logical_top,
                                    LogicalExtentComputedValues&) const;
  // This function will compute the logical border-box height, without laying
  // out the box. This means that the result is only "correct" when the height
  // is explicitly specified. This function exists so that intrinsic width
  // calculations have a way to deal with children that have orthogonal flows.
  // When there is no explicit height, this function assumes a content height of
  // zero (and returns just border+padding).
  LayoutUnit ComputeLogicalHeightWithoutLayout() const;

  bool StretchesToViewport() const {
    NOT_DESTROYED();
    return GetDocument().InQuirksMode() && StretchesToViewportInQuirksMode();
  }

  virtual LayoutSize IntrinsicSize() const {
    NOT_DESTROYED();
    return LayoutSize();
  }
  LayoutUnit IntrinsicLogicalWidth() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? IntrinsicSize().Width()
                                                : IntrinsicSize().Height();
  }
  LayoutUnit IntrinsicLogicalHeight() const {
    NOT_DESTROYED();
    return StyleRef().IsHorizontalWritingMode() ? IntrinsicSize().Height()
                                                : IntrinsicSize().Width();
  }

  // Whether or not the element shrinks to its intrinsic width (rather than
  // filling the width of a containing block). HTML4 buttons, <select>s,
  // <input>s, legends, and floating/compact elements do this.
  bool SizesLogicalWidthToFitContent(const Length& logical_width) const;

  bool AutoWidthShouldFitContent() const;

  LayoutUnit ComputeLogicalHeightUsing(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeContentLogicalHeight(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeContentAndScrollbarLogicalHeightUsing(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeReplacedLogicalHeightUsing(SizeType, Length height) const;
  LayoutUnit ComputeReplacedLogicalHeightRespectingMinMaxHeight(
      LayoutUnit logical_height) const;

  virtual LayoutUnit ComputeReplacedLogicalHeight(
      LayoutUnit estimated_used_width = LayoutUnit()) const;

  virtual bool ShouldComputeSizeAsReplaced() const {
    NOT_DESTROYED();
    return IsAtomicInlineLevel() && !IsInlineBlockOrInlineTable();
  }

  // Returns the size that percentage logical heights of this box should be
  // resolved against. This function will walk the ancestor chain of this
  // object to determine this size.
  //  - out_cb returns the LayoutBlock which provided the size.
  //  - out_skipped_auto_height_containing_block returns if any auto height
  //    blocks were skipped to obtain out_cb.
  LayoutUnit ContainingBlockLogicalHeightForPercentageResolution(
      LayoutBlock** out_cb = nullptr,
      bool* out_skipped_auto_height_containing_block = nullptr) const;

  LayoutUnit ComputePercentageLogicalHeight(const Length& height) const;

  // Block flows subclass availableWidth/Height to handle multi column layout
  // (shrinking the width/height available to children when laying out.)
  LayoutUnit AvailableLogicalWidth() const {
    NOT_DESTROYED();
    return ContentLogicalWidth();
  }
  LayoutUnit AvailableLogicalHeight(AvailableLogicalHeightType) const;
  LayoutUnit AvailableLogicalHeightUsing(const Length&,
                                         AvailableLogicalHeightType) const;

  // Return both scrollbars and scrollbar gutters (defined by scrollbar-gutter).
  inline NGPhysicalBoxStrut ComputeScrollbars() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars())
      return NGPhysicalBoxStrut();
    else
      return ComputeScrollbarsInternal();
  }
  inline NGBoxStrut ComputeLogicalScrollbars() const {
    NOT_DESTROYED();
    if (CanSkipComputeScrollbars()) {
      return NGBoxStrut();
    } else {
      return ComputeScrollbarsInternal().ConvertToLogical(
          StyleRef().GetWritingDirection());
    }
  }

  bool CanBeScrolledAndHasScrollableArea() const;
  virtual bool CanBeProgrammaticallyScrolled() const;
  virtual void Autoscroll(const PhysicalOffset&);
  PhysicalOffset CalculateAutoscrollDirection(
      const gfx::PointF& point_in_root_frame) const;
  static LayoutBox* FindAutoscrollable(LayoutObject*,
                                       bool is_middle_click_autoscroll);
  static bool HasHorizontallyScrollableAncestor(LayoutObject*);

  DISABLE_CFI_PERF bool HasAutoVerticalScrollbar() const {
    NOT_DESTROYED();
    return HasNonVisibleOverflow() && StyleRef().HasAutoVerticalScroll();
  }
  DISABLE_CFI_PERF bool HasAutoHorizontalScrollbar() const {
    NOT_DESTROYED();
    return HasNonVisibleOverflow() && StyleRef().HasAutoHorizontalScroll();
  }
  DISABLE_CFI_PERF bool ScrollsOverflow() const {
    NOT_DESTROYED();
    return HasNonVisibleOverflow() && StyleRef().ScrollsOverflow();
  }
  // We place block-direction scrollbar on the left only if the writing-mode
  // is horizontal, so ShouldPlaceVerticalScrollbarOnLeft() is the same as
  // ShouldPlaceBlockDirectionScrollbarOnLogicalLeft(). The two forms can be
  // used in different contexts, e.g. the former for physical coordinate
  // contexts, and the later for logical coordinate contexts.
  bool ShouldPlaceVerticalScrollbarOnLeft() const {
    NOT_DESTROYED();
    return ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
  }
  virtual bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    NOT_DESTROYED();
    return StyleRef().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
  }

  bool HasScrollableOverflowX() const {
    NOT_DESTROYED();
    return ScrollsOverflowX() &&
           PixelSnappedScrollWidth() != PixelSnappedClientWidth();
  }
  bool HasScrollableOverflowY() const {
    NOT_DESTROYED();
    return ScrollsOverflowY() &&
           PixelSnappedScrollHeight() != PixelSnappedClientHeight();
  }
  virtual bool ScrollsOverflowX() const {
    NOT_DESTROYED();
    return HasNonVisibleOverflow() && StyleRef().ScrollsOverflowX();
  }
  virtual bool ScrollsOverflowY() const {
    NOT_DESTROYED();
    return HasNonVisibleOverflow() && StyleRef().ScrollsOverflowY();
  }

  // Elements such as the <input> field override this to specify that they are
  // scrollable outside the context of the CSS overflow style
  virtual bool IsIntrinsicallyScrollable(
      ScrollbarOrientation orientation) const {
    NOT_DESTROYED();
    return false;
  }

  // Return true if this box is monolithic, i.e. unbreakable in a fragmentation
  // context.
  virtual bool IsMonolithic() const;

  bool HasUnsplittableScrollingOverflow() const;

  LayoutRect LocalCaretRect(
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const override;

  // Returns the intersection of all overflow clips which apply.
  virtual PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  PhysicalRect ClipRect(const PhysicalOffset& location) const;

  // This version is for legacy code that has not switched to the new physical
  // geometry yet.
  LayoutRect OverflowClipRect(const LayoutPoint& location,
                              OverlayScrollbarClipBehavior behavior =
                                  kIgnoreOverlayScrollbarSize) const {
    NOT_DESTROYED();
    return OverflowClipRect(PhysicalOffset(location), behavior).ToLayoutRect();
  }

  // Returns the combination of overflow clip, contain: paint clip and CSS clip
  // for this object.
  PhysicalRect ClippingRect(const PhysicalOffset& location) const;

  virtual void PaintBoxDecorationBackground(
      const PaintInfo&,
      const PhysicalOffset& paint_offset) const;
  virtual void PaintMask(const PaintInfo&,
                         const PhysicalOffset& paint_offset) const;
  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;
  ResourcePriority ComputeResourcePriority() const final;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;
  PositionWithAffinity PositionForPointInFragments(const PhysicalOffset&) const;

  void RemoveFloatingOrPositionedChildFromBlockLists();

  bool ShrinkToAvoidFloats() const;
  virtual bool CreatesNewFormattingContext() const {
    NOT_DESTROYED();
    return true;
  }
  bool ShouldBeConsideredAsReplaced() const;

  // Return true if this block establishes a fragmentation context root (e.g. a
  // multicol container).
  virtual bool IsFragmentationContextRoot() const {
    NOT_DESTROYED();
    return false;
  }

  bool IsWritingModeRoot() const {
    NOT_DESTROYED();
    return !Parent() ||
           Parent()->StyleRef().GetWritingMode() != StyleRef().GetWritingMode();
  }
  bool IsOrthogonalWritingModeRoot() const {
    NOT_DESTROYED();
    return Parent() &&
           Parent()->IsHorizontalWritingMode() != IsHorizontalWritingMode();
  }
  void MarkOrthogonalWritingModeRoot();
  void UnmarkOrthogonalWritingModeRoot();

  bool IsCustomItem() const;
  bool IsCustomItemShrinkToFit() const;

  // TODO(1229581): Rename this function.
  bool IsFlexItemIncludingNG() const {
    NOT_DESTROYED();
    return !IsInline() && !IsOutOfFlowPositioned() && Parent() &&
           Parent()->IsFlexibleBoxIncludingNG();
  }

  // TODO(1229581): Rename this function.
  bool IsGridItemIncludingNG() const {
    NOT_DESTROYED();
    return Parent() && Parent()->IsLayoutNGGrid();
  }

  bool IsMathItem() const {
    NOT_DESTROYED();
    return Parent() && Parent()->IsMathML();
  }

  LayoutUnit FirstLineHeight() const override;

  PhysicalOffset OffsetPoint(const Element* parent) const;
  LayoutUnit OffsetLeft(const Element*) const final;
  LayoutUnit OffsetTop(const Element*) const final;

  [[nodiscard]] LayoutUnit FlipForWritingMode(
      LayoutUnit position,
      LayoutUnit width = LayoutUnit()) const {
    NOT_DESTROYED();
    // The offset is in the block direction (y for horizontal writing modes, x
    // for vertical writing modes).
    if (LIKELY(!HasFlippedBlocksWritingMode()))
      return position;
    DCHECK(!IsHorizontalWritingMode());
    return Size().Width() - (position + width);
  }
  // Inherit other flipping methods from LayoutObject.
  using LayoutObject::FlipForWritingMode;

  [[nodiscard]] LayoutPoint DeprecatedFlipForWritingMode(
      const LayoutPoint& position) const {
    NOT_DESTROYED();
    return LayoutPoint(FlipForWritingMode(position.X()), position.Y());
  }
  void DeprecatedFlipForWritingMode(LayoutRect& rect) const {
    NOT_DESTROYED();
    if (LIKELY(!HasFlippedBlocksWritingMode()))
      return;
    rect = FlipForWritingMode(rect).ToLayoutRect();
  }

  // Passing |flipped_blocks_container| causes flipped-block flipping w.r.t.
  // that container, or LocationContainer() otherwise.
  PhysicalOffset PhysicalLocation(
      const LayoutBox* flipped_blocks_container = nullptr) const {
    NOT_DESTROYED();
    return PhysicalLocationInternal(flipped_blocks_container
                                        ? flipped_blocks_container
                                        : LocationContainer());
  }

  // Convert a local rect in this box's blocks direction into parent's blocks
  // direction, for parent to accumulate layout or visual overflow.
  LayoutRect RectForOverflowPropagation(const LayoutRect&) const;

  LayoutRect VisualOverflowRectForPropagation() const {
    NOT_DESTROYED();
    return RectForOverflowPropagation(VisualOverflowRect());
  }
  LayoutRect LogicalLayoutOverflowRectForPropagation(
      LayoutObject* container) const;
  LayoutRect LayoutOverflowRectForPropagation(LayoutObject* container) const;

  bool HasSelfVisualOverflow() const {
    NOT_DESTROYED();
    return VisualOverflowIsSet() &&
           !BorderBoxRect().Contains(
               overflow_->visual_overflow->SelfVisualOverflowRect());
  }

  bool HasVisualOverflow() const {
    NOT_DESTROYED();
    return VisualOverflowIsSet();
  }
  bool HasLayoutOverflow() const {
    NOT_DESTROYED();
    return LayoutOverflowIsSet();
  }

  // Return true if re-laying out the containing block of this object means that
  // we need to recalculate the preferred min/max logical widths of this object.
  //
  // Calculating min/max widths for an object should ideally only take itself
  // and its children as input. However, some objects don't adhere strictly to
  // this rule, and also take input from their containing block to figure out
  // their min/max widths. This is the case for e.g. shrink-to-fit containers
  // with percentage inline-axis padding. This isn't good practise, but that's
  // how it is and how it's going to stay, unless we want to undertake a
  // substantial maintenance task of the min/max preferred widths machinery.
  virtual bool NeedsPreferredWidthsRecalculation() const;

  // See README.md for an explanation of scroll origin.
  gfx::Vector2d OriginAdjustmentForScrollbars() const;
  gfx::Point ScrollOrigin() const;
  PhysicalOffset ScrolledContentOffset() const;

  // Scroll offset as snapped to physical pixels. This value should be used in
  // any values used after layout and inside "layout code" that cares about
  // where the content is displayed, rather than what the ideal offset is. For
  // most other cases ScrolledContentOffset is probably more appropriate. This
  // is the offset that's actually drawn to the screen.
  // TODO(crbug.com/962299): Pixel-snapping before PrePaint (when we know the
  // paint offset) is incorrect.
  gfx::Vector2d PixelSnappedScrolledContentOffset() const;

  // Maps from scrolling contents space to box space and apply overflow
  // clip if needed. Returns true if no clipping applied or the flattened quad
  // bounds actually intersects the clipping region. If edgeInclusive is true,
  // then this method may return true even if the resulting rect has zero area.
  //
  // When applying offsets and not clips, the TransformAccumulation is
  // respected. If there is a clip, the TransformState is flattened first.
  bool MapContentsRectToBoxSpace(
      TransformState&,
      TransformState::TransformAccumulation,
      const LayoutObject& contents,
      VisualRectFlags = kDefaultVisualRectFlags) const;

  // True if the contents scroll relative to this object. |this| must be a
  // containing block for |contents|.
  bool ContainedContentsScroll(const LayoutObject& contents) const;

  // Applies the box clip. This is like mapScrollingContentsRectToBoxSpace,
  // except it does not apply scroll.
  bool ApplyBoxClips(TransformState&,
                     TransformState::TransformAccumulation,
                     VisualRectFlags) const;

  // The optional |size| parameter is used if the size of the object isn't
  // correct yet.
  gfx::PointF PerspectiveOrigin(const PhysicalSize* size = nullptr) const;

  // Maps the visual rect state |transform_state| from this box into its
  // container, applying adjustments for the given container offset,
  // scrolling, container clipping, and transform (including container
  // perspective).
  bool MapVisualRectToContainer(const LayoutObject* container_object,
                                const PhysicalOffset& container_offset,
                                const LayoutObject* ancestor,
                                VisualRectFlags,
                                TransformState&) const;

  bool HasRelativeLogicalHeight() const;

  virtual LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject*) const {
    NOT_DESTROYED();
    NOTREACHED();
    return nullptr;
  }

  bool HasSameDirectionAs(const LayoutBox* object) const {
    NOT_DESTROYED();
    return StyleRef().Direction() == object->StyleRef().Direction();
  }

  ShapeOutsideInfo* GetShapeOutsideInfo() const;

  bool CanRenderBorderImage() const;

  // For snap areas, returns the snap container that owns us.
  LayoutBox* SnapContainer() const;
  void SetSnapContainer(LayoutBox*);
  // For snap containers, returns all associated snap areas.
  SnapAreaSet* SnapAreas() const;
  void ClearSnapAreas();
  // Moves all snap areas to the new container.
  void ReassignSnapAreas(LayoutBox& new_container);

  // CustomLayoutChild only exists if this LayoutBox is a IsCustomItem (aka. a
  // child of a LayoutCustom). This is created/destroyed when this LayoutBox is
  // inserted/removed from the layout tree.
  CustomLayoutChild* GetCustomLayoutChild() const;
  void AddCustomLayoutChildIfNeeded();
  void ClearCustomLayoutChild();

  bool HitTestClippedOutByBorder(
      const HitTestLocation&,
      const PhysicalOffset& border_box_location) const;

  bool HitTestOverflowControl(HitTestResult&,
                              const HitTestLocation&,
                              const PhysicalOffset&) const;

  // Returns true if the box intersects the viewport visible to the user.
  bool IntersectsVisibleViewport() const;

  void EnsureIsReadyForPaintInvalidation() override;
  void ClearPaintFlags() override;

  bool HasControlClip() const;

  class MutableForPainting : public LayoutObject::MutableForPainting {
   public:
    void SavePreviousSize() {
      GetLayoutBox().previous_size_ = GetLayoutBox().Size();
    }
    void ClearPreviousSize() { GetLayoutBox().previous_size_ = LayoutSize(); }
    void SavePreviousOverflowData();
    void ClearPreviousOverflowData() {
      DCHECK(!GetLayoutBox().HasVisualOverflow());
      DCHECK(!GetLayoutBox().HasLayoutOverflow());
      GetLayoutBox().overflow_.reset();
    }
    void SavePreviousContentBoxRect() {
      auto& rare_data = GetLayoutBox().EnsureRareData();
      rare_data.has_previous_content_box_rect_ = true;
      rare_data.previous_physical_content_box_rect_ =
          GetLayoutBox().PhysicalContentBoxRect();
    }
    void ClearPreviousContentBoxRect() {
      if (auto* rare_data = GetLayoutBox().rare_data_.Get())
        rare_data->has_previous_content_box_rect_ = false;
    }

    // Called from LayoutShiftTracker when we attach this LayoutBox to a node
    // for which we saved these values when the node was detached from its
    // original LayoutBox.
    void SetPreviousGeometryForLayoutShiftTracking(
        const PhysicalOffset& paint_offset,
        const LayoutSize& size,
        const PhysicalRect& visual_overflow_rect);

   protected:
    friend class LayoutBox;
    MutableForPainting(const LayoutBox& box)
        : LayoutObject::MutableForPainting(box) {}
    LayoutBox& GetLayoutBox() {
      return static_cast<LayoutBox&>(layout_object_);
    }
  };

  MutableForPainting GetMutableForPainting() const {
    NOT_DESTROYED();
    return MutableForPainting(*this);
  }

  LayoutSize PreviousSize() const {
    NOT_DESTROYED();
    return previous_size_;
  }
  PhysicalRect PreviousPhysicalContentBoxRect() const {
    NOT_DESTROYED();
    return rare_data_ && rare_data_->has_previous_content_box_rect_
               ? rare_data_->previous_physical_content_box_rect_
               : PhysicalRect(PhysicalOffset(), PreviousSize());
  }
  PhysicalRect PreviousPhysicalVisualOverflowRect() const {
    NOT_DESTROYED();
    return overflow_ && overflow_->previous_overflow_data
               ? overflow_->previous_overflow_data
                     ->previous_physical_visual_overflow_rect
               : PhysicalRect(PhysicalOffset(), PreviousSize());
  }
  PhysicalRect PreviousPhysicalLayoutOverflowRect() const {
    NOT_DESTROYED();
    return overflow_ && overflow_->previous_overflow_data
               ? overflow_->previous_overflow_data
                     ->previous_physical_layout_overflow_rect
               : PhysicalRect(PhysicalOffset(), PreviousSize());
  }
  PhysicalRect PreviousPhysicalSelfVisualOverflowRect() const {
    NOT_DESTROYED();
    return overflow_ && overflow_->previous_overflow_data
               ? overflow_->previous_overflow_data
                     ->previous_physical_self_visual_overflow_rect
               : PhysicalRect(PhysicalOffset(), PreviousSize());
  }

  // Calculates the intrinsic logical widths for this layout box.
  // https://drafts.csswg.org/css-sizing-3/#intrinsic
  //
  // intrinsicWidth is defined as:
  //     intrinsic size of content (with our border and padding) +
  //     scrollbarWidth.
  //
  // preferredWidth is defined as:
  //     fixedWidth OR (intrinsicWidth plus border and padding).
  //     Note: fixedWidth includes border and padding and scrollbarWidth.
  //
  // This is public only for use by LayoutNG. Do not call this elsewhere.
  virtual MinMaxSizes ComputeIntrinsicLogicalWidths() const = 0;

  // Returns the (maybe cached) intrinsic logical widths for this layout box.
  MinMaxSizes IntrinsicLogicalWidths(
      MinMaxSizesType type = MinMaxSizesType::kContent) const;

  // If |IntrinsicLogicalWidthsDirty()| is true, recalculates the intrinsic
  // logical widths.
  void UpdateCachedIntrinsicLogicalWidthsIfNeeded();

  // LayoutNG can use this function to update our cache of intrinsic logical
  // widths when the layout object is managed by NG. Should not be called by
  // regular code.
  //
  // Also clears the "dirty" flag for the intrinsic logical widths.
  void SetIntrinsicLogicalWidthsFromNG(
      LayoutUnit intrinsic_logical_widths_initial_block_size,
      bool depends_on_block_constraints,
      bool child_depends_on_block_constraints,
      const MinMaxSizes* sizes) {
    NOT_DESTROYED();
    intrinsic_logical_widths_initial_block_size_ =
        intrinsic_logical_widths_initial_block_size;
    SetIntrinsicLogicalWidthsDependsOnBlockConstraints(
        depends_on_block_constraints);
    SetIntrinsicLogicalWidthsChildDependsOnBlockConstraints(
        child_depends_on_block_constraints);
    if (sizes)
      intrinsic_logical_widths_ = *sizes;
    ClearIntrinsicLogicalWidthsDirty();
  }

  // Returns what initial block-size was used in the intrinsic logical widths
  // phase. This is used for caching purposes when %-block-size children with
  // aspect-ratios are present.
  //
  // For non-LayoutNG code this is always LayoutUnit::Min(), and should not be
  // used for caching purposes.
  LayoutUnit IntrinsicLogicalWidthsInitialBlockSize() const {
    NOT_DESTROYED();
    return intrinsic_logical_widths_initial_block_size_;
  }

  // Make it public.
  using LayoutObject::BackgroundIsKnownToBeObscured;

  // Sets the coordinates of find-in-page scrollbar tickmarks, bypassing
  // DocumentMarkerController.  This is used by the PDF plugin.
  void OverrideTickmarks(Vector<gfx::Rect> tickmarks);

  // Issues a paint invalidation on the layout viewport's vertical scrollbar
  // (which is responsible for painting the tickmarks).
  void InvalidatePaintForTickmarks();

  // Returns which of the border box space and contents space (maybe both)
  // the backgrounds should be painted into, if the LayoutBox is composited.
  // The caller may adjust the value by considering LCD-text etc. if needed and
  // call SetBackgroundPaintLocation() with the value to be used for painting.
  BackgroundPaintLocation ComputeBackgroundPaintLocationIfComposited() const;

  bool HasFragmentItems() const {
    NOT_DESTROYED();
    return ChildrenInline() && PhysicalFragments().HasFragmentItems();
  }

  // Returns true if this box is fixed position and will not move with
  // scrolling. If the caller can pre-calculate |container_for_fixed_position|,
  // it should pass it to avoid recalculation.
  bool IsFixedToView(
      const LayoutObject* container_for_fixed_position = nullptr) const;

  // See StickyPositionScrollingConstraints::constraining_rect.
  PhysicalRect ComputeStickyConstrainingRect() const;

  bool HasAnchorScrollTranslation() const;
  PhysicalOffset AnchorScrollTranslationOffset() const;

  bool HasScrollbarGutters(ScrollbarOrientation orientation) const;

  // This should be called when the border-box size of this box is changed.
  void SizeChanged();

 protected:
  ~LayoutBox() override;

  virtual OverflowClipAxes ComputeOverflowClipAxes() const;

  void WillBeDestroyed() override;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void UpdateFromStyle() override;

  void InLayoutNGInlineFormattingContextWillChange(bool) final;

  virtual ItemPosition SelfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const {
    NOT_DESTROYED();
    DCHECK(!child);
    return ItemPosition::kStretch;
  }

  PhysicalRect BackgroundPaintedExtent() const;
  virtual bool ForegroundIsKnownToBeOpaqueInRect(
      const PhysicalRect& local_rect,
      unsigned max_depth_to_test) const;
  virtual bool ComputeBackgroundIsKnownToBeObscured() const;
  bool ComputeCanCompositeBackgroundAttachmentFixed() const override;

  LayoutUnit ComputeIntrinsicLogicalContentHeightUsing(
      SizeType height_type,
      const Length& logical_height_length,
      LayoutUnit intrinsic_content_height,
      LayoutUnit border_and_padding) const;

  virtual bool HitTestChildren(HitTestResult&,
                               const HitTestLocation&,
                               const PhysicalOffset& accumulated_offset,
                               HitTestPhase);

  void InvalidatePaint(const PaintInvalidatorContext&) const override;

  bool ColumnFlexItemHasStretchAlignment() const;
  bool IsStretchingColumnFlexItem() const;

  void ExcludeScrollbars(
      PhysicalRect&,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize,
      ShouldIncludeScrollbarGutter = kIncludeScrollbarGutter) const;

  LayoutUnit ContainingBlockLogicalWidthForPositioned(
      const LayoutBoxModelObject* containing_block,
      bool check_for_perpendicular_writing_mode = true) const;
  LayoutUnit ContainingBlockLogicalHeightForPositioned(
      const LayoutBoxModelObject* containing_block,
      bool check_for_perpendicular_writing_mode = true) const;

  static void ComputeBlockStaticDistance(
      Length& logical_top,
      Length& logical_bottom,
      const LayoutBox* child,
      const LayoutBoxModelObject* container_block,
      const NGBoxFragmentBuilder* = nullptr);
  static void ComputeInlineStaticDistance(
      Length& logical_left,
      Length& logical_right,
      const LayoutBox* child,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_width,
      const NGBoxFragmentBuilder* = nullptr);
  static void ComputeLogicalTopPositionedOffset(
      LayoutUnit& logical_top_pos,
      const LayoutBox* child,
      LayoutUnit logical_height_value,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_height);
  static bool SkipContainingBlockForPercentHeightCalculation(
      const LayoutBox* containing_block);

  PhysicalRect LocalVisualRectIgnoringVisibility() const override;

  PhysicalOffset OffsetFromContainerInternal(
      const LayoutObject*,
      bool ignore_scroll_offset) const override;

  // For atomic inlines, returns its resolved direction in text flow. Not to be
  // confused with the CSS property 'direction'.
  // Returns the CSS 'direction' property value when it is not atomic inline.
  TextDirection ResolvedDirection() const;

  // RecalcLayoutOverflow implementations for LayoutNG.
  RecalcLayoutOverflowResult RecalcLayoutOverflowNG();
  RecalcLayoutOverflowResult RecalcChildLayoutOverflowNG();

 private:
  inline bool LayoutOverflowIsSet() const {
    NOT_DESTROYED();
    return overflow_ && overflow_->layout_overflow;
  }
#if DCHECK_IS_ON()
  void CheckIsVisualOverflowComputed() const;
#else
  ALWAYS_INLINE void CheckIsVisualOverflowComputed() const { NOT_DESTROYED(); }
#endif
  inline bool VisualOverflowIsSet() const {
    NOT_DESTROYED();
    CheckIsVisualOverflowComputed();
    return overflow_ && overflow_->visual_overflow;
  }

  // The outsets from this box's border-box that the element's content should be
  // clipped to, including overflow-clip-margin.
  NGPhysicalBoxStrut BorderBoxOutsetsForClipping() const;

  void UpdateHasSubpixelVisualEffectOutsets(const NGPhysicalBoxStrut&);
  void SetVisualOverflow(const PhysicalRect& self,
                         const PhysicalRect& contents);
  void CopyVisualOverflowFromFragmentsWithoutInvalidations();

  void UpdateShapeOutsideInfoAfterStyleChange(const ComputedStyle&,
                                              const ComputedStyle* old_style);
  void UpdateGridPositionAfterStyleChange(const ComputedStyle*);
  void UpdateScrollSnapMappingAfterStyleChange(const ComputedStyle& old_style);
  void ClearScrollSnapMapping();
  void AddScrollSnapMapping();

  bool StretchesToViewportInQuirksMode() const;

  virtual void ComputePositionedLogicalHeight(
      LogicalExtentComputedValues&) const;
  void ComputePositionedLogicalWidthUsing(
      SizeType,
      const Length& logical_width,
      const LayoutBoxModelObject* container_block,
      TextDirection container_direction,
      LayoutUnit container_logical_width,
      LayoutUnit borders_plus_padding,
      const Length& logical_left,
      const Length& logical_right,
      const Length& margin_logical_left,
      const Length& margin_logical_right,
      LogicalExtentComputedValues&) const;
  void ComputePositionedLogicalHeightUsing(
      SizeType,
      Length logical_height_length,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_height,
      LayoutUnit borders_plus_padding,
      LayoutUnit logical_height,
      const Length& logical_top,
      const Length& logical_bottom,
      const Length& margin_logical_top,
      const Length& margin_logical_bottom,
      LogicalExtentComputedValues&) const;

  LayoutBoxRareData& EnsureRareData() {
    NOT_DESTROYED();
    if (!rare_data_)
      rare_data_ = MakeGarbageCollected<LayoutBoxRareData>();
    return *rare_data_.Get();
  }

  bool LogicalHeightComputesAsNone(SizeType) const;

  bool IsBox() const =
      delete;  // This will catch anyone doing an unnecessary check.

  void LocationChanged();

  void InflateVisualRectForFilter(TransformState&) const;
  void InflateVisualRectForFilterUnderContainer(
      TransformState&,
      const LayoutObject& container,
      const LayoutBoxModelObject* ancestor_to_stop_at) const;

  NGPhysicalBoxStrut margin_box_outsets_;

  void AddSnapArea(LayoutBox&);
  void RemoveSnapArea(const LayoutBox&);

  PhysicalRect DebugRect() const override;

  RasterEffectOutset VisualRectOutsetForRasterEffects() const override;

  inline bool CanSkipComputeScrollbars() const {
    NOT_DESTROYED();
    return (StyleRef().IsOverflowVisibleAlongBothAxes() ||
            !HasNonVisibleOverflow() ||
            (GetScrollableArea() &&
             !GetScrollableArea()->HasHorizontalScrollbar() &&
             !GetScrollableArea()->HasVerticalScrollbar())) &&
           StyleRef().IsScrollbarGutterAuto();
  }

  NGPhysicalBoxStrut ComputeScrollbarsInternal(
      ShouldClampToContentBox = kDoNotClampToContentBox,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize,
      ShouldIncludeScrollbarGutter = kIncludeScrollbarGutter) const;

  LayoutUnit FlipForWritingModeInternal(
      LayoutUnit position,
      LayoutUnit width,
      const LayoutBox* box_for_flipping) const final {
    NOT_DESTROYED();
    DCHECK(!box_for_flipping || box_for_flipping == this);
    return FlipForWritingMode(position, width);
  }

  PhysicalOffset PhysicalLocationInternal(
      const LayoutBox* container_box) const {
    NOT_DESTROYED();
    DCHECK_EQ(container_box, LocationContainer());
    LayoutPoint location = Location();
    if (LIKELY(!container_box || !container_box->HasFlippedBlocksWritingMode()))
      return PhysicalOffset(location);

    return PhysicalOffset(
        container_box->Size().Width() - Size().Width() - location.X(),
        location.Y());
  }

  bool BackgroundClipBorderBoxIsEquivalentToPaddingBox() const;

  // Compute the border-box size from physical fragments.
  LayoutSize ComputeSize() const;
  void InvalidateCachedGeometry();

  // Clear LayoutObject fields of physical fragments.
  void DisassociatePhysicalFragments();

 protected:
  // The CSS border box rect for this box.
  //
  // The rectangle is in LocationContainer's physical coordinates in flipped
  // block-flow direction of LocationContainer (see the COORDINATE SYSTEMS
  // section in LayoutBoxModelObject). The location is the distance from this
  // object's border edge to the LocationContainer's border edge. Thus it
  // includes any logical top/left along with this box's margins. It doesn't
  // include transforms, relative position offsets etc.
  LayoutPoint frame_location_;

  // TODO(crbug.com/1353190): Remove frame_size_.
  LayoutSize frame_size_;

 private:
  // Previous value of frame_size_, updated after paint invalidation.
  LayoutSize previous_size_;

 protected:
  MinMaxSizes intrinsic_logical_widths_;
  LayoutUnit intrinsic_logical_widths_initial_block_size_;

  Member<const NGLayoutResult> measure_result_;
  NGLayoutResultList layout_results_;

  // LayoutBoxUtils is used for the LayoutNG code querying protected methods on
  // this class, e.g. determining the static-position of OOF elements.
  friend class LayoutBoxUtils;
  friend class LayoutBoxTest;

 private:
  std::unique_ptr<BoxOverflowModel> overflow_;

  // Extra layout input data. This one may be set during layout, and cleared
  // afterwards. Always nullptr when this object isn't in the process of being
  // laid out.
  const BoxLayoutExtraInput* extra_input_ = nullptr;

  // The index of the first fragment item associated with this object in
  // |NGFragmentItems::Items()|. Zero means there are no such item.
  // Valid only when IsInLayoutNGInlineFormattingContext().
  wtf_size_t first_fragment_item_index_ = 0u;

  Member<LayoutBoxRareData> rare_data_;

  FRIEND_TEST_ALL_PREFIXES(LayoutMultiColumnSetTest, ScrollAnchroingCrash);
};

template <>
struct DowncastTraits<LayoutBox> {
  static bool AllowFrom(const LayoutObject& object) { return object.IsBox(); }
};

inline LayoutBox* LayoutBox::PreviousSiblingBox() const {
  return To<LayoutBox>(PreviousSibling());
}

inline LayoutBox* LayoutBox::PreviousInFlowSiblingBox() const {
  LayoutBox* previous = PreviousSiblingBox();
  while (previous && previous->IsOutOfFlowPositioned())
    previous = previous->PreviousSiblingBox();
  return previous;
}

inline LayoutBox* LayoutBox::NextSiblingBox() const {
  return To<LayoutBox>(NextSibling());
}

inline LayoutBox* LayoutBox::NextInFlowSiblingBox() const {
  LayoutBox* next = NextSiblingBox();
  while (next && next->IsOutOfFlowPositioned())
    next = next->NextSiblingBox();
  return next;
}

inline LayoutBox* LayoutBox::ParentBox() const {
  return To<LayoutBox>(Parent());
}

inline LayoutBox* LayoutBox::FirstInFlowChildBox() const {
  LayoutBox* first = FirstChildBox();
  return (first && first->IsOutOfFlowPositioned())
             ? first->NextInFlowSiblingBox()
             : first;
}

inline LayoutBox* LayoutBox::FirstChildBox() const {
  return To<LayoutBox>(SlowFirstChild());
}

inline LayoutBox* LayoutBox::LastChildBox() const {
  return To<LayoutBox>(SlowLastChild());
}

inline LayoutBox* LayoutBox::PreviousSiblingMultiColumnBox() const {
  DCHECK(IsLayoutMultiColumnSpannerPlaceholder() || IsLayoutMultiColumnSet());
  LayoutBox* previous_box = PreviousSiblingBox();
  if (previous_box->IsLayoutFlowThread())
    return nullptr;
  return previous_box;
}

inline LayoutBox* LayoutBox::NextSiblingMultiColumnBox() const {
  DCHECK(IsLayoutMultiColumnSpannerPlaceholder() || IsLayoutMultiColumnSet());
  return NextSiblingBox();
}

inline wtf_size_t LayoutBox::FirstInlineFragmentItemIndex() const {
  if (!IsInLayoutNGInlineFormattingContext())
    return 0u;
  return first_fragment_item_index_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_
