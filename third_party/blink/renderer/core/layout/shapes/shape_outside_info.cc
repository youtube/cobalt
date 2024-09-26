/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"

#include <memory>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

CSSBoxType ReferenceBox(const ShapeValue& shape_value) {
  if (shape_value.CssBox() == CSSBoxType::kMissing)
    return CSSBoxType::kMargin;
  return shape_value.CssBox();
}

void ShapeOutsideInfo::SetReferenceBoxLogicalSize(
    LayoutSize new_reference_box_logical_size) {
  Document& document = layout_box_->GetDocument();
  bool is_horizontal_writing_mode =
      layout_box_->ContainingBlock()->StyleRef().IsHorizontalWritingMode();

  LayoutSize margin_box_for_use_counter = new_reference_box_logical_size;
  if (is_horizontal_writing_mode) {
    margin_box_for_use_counter.Expand(layout_box_->MarginWidth(),
                                      layout_box_->MarginHeight());
  } else {
    margin_box_for_use_counter.Expand(layout_box_->MarginHeight(),
                                      layout_box_->MarginWidth());
  }

  const ShapeValue& shape_value = *layout_box_->StyleRef().ShapeOutside();
  switch (ReferenceBox(shape_value)) {
    case CSSBoxType::kMargin:
      UseCounter::Count(document, WebFeature::kShapeOutsideMarginBox);
      if (is_horizontal_writing_mode) {
        new_reference_box_logical_size.Expand(layout_box_->MarginWidth(),
                                              layout_box_->MarginHeight());
      } else {
        new_reference_box_logical_size.Expand(layout_box_->MarginHeight(),
                                              layout_box_->MarginWidth());
      }
      break;
    case CSSBoxType::kBorder:
      UseCounter::Count(document, WebFeature::kShapeOutsideBorderBox);
      break;
    case CSSBoxType::kPadding:
      UseCounter::Count(document, WebFeature::kShapeOutsidePaddingBox);
      if (is_horizontal_writing_mode) {
        new_reference_box_logical_size.Shrink(layout_box_->BorderWidth(),
                                              layout_box_->BorderHeight());
      } else {
        new_reference_box_logical_size.Shrink(layout_box_->BorderHeight(),
                                              layout_box_->BorderWidth());
      }

      if (new_reference_box_logical_size != margin_box_for_use_counter) {
        UseCounter::Count(
            document,
            WebFeature::kShapeOutsidePaddingBoxDifferentFromMarginBox);
      }
      break;
    case CSSBoxType::kContent: {
      bool is_shape_image = shape_value.GetType() == ShapeValue::kImage;

      if (!is_shape_image)
        UseCounter::Count(document, WebFeature::kShapeOutsideContentBox);

      if (is_horizontal_writing_mode) {
        new_reference_box_logical_size.Shrink(
            layout_box_->BorderAndPaddingWidth(),
            layout_box_->BorderAndPaddingHeight());
      } else {
        new_reference_box_logical_size.Shrink(
            layout_box_->BorderAndPaddingHeight(),
            layout_box_->BorderAndPaddingWidth());
      }

      if (!is_shape_image &&
          new_reference_box_logical_size != margin_box_for_use_counter) {
        UseCounter::Count(
            document,
            WebFeature::kShapeOutsideContentBoxDifferentFromMarginBox);
      }
      break;
    }
    case CSSBoxType::kMissing:
      NOTREACHED();
      break;
  }

  new_reference_box_logical_size.ClampNegativeToZero();

  if (reference_box_logical_size_ == new_reference_box_logical_size)
    return;
  MarkShapeAsDirty();
  reference_box_logical_size_ = new_reference_box_logical_size;
}

void ShapeOutsideInfo::SetPercentageResolutionInlineSize(
    LayoutUnit percentage_resolution_inline_size) {
  if (percentage_resolution_inline_size_ == percentage_resolution_inline_size)
    return;

  MarkShapeAsDirty();
  percentage_resolution_inline_size_ = percentage_resolution_inline_size;
}

static bool CheckShapeImageOrigin(Document& document,
                                  const StyleImage& style_image) {
  String failing_url;
  if (style_image.IsAccessAllowed(failing_url))
    return true;
  String url_string = failing_url.IsNull() ? "''" : failing_url;
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError,
      "Unsafe attempt to load URL " + url_string + "."));
  return false;
}

static LayoutRect GetShapeImageMarginRect(
    const LayoutBox& layout_box,
    const LayoutSize& reference_box_logical_size) {
  LayoutPoint margin_box_origin(
      -layout_box.MarginLineLeft() - layout_box.BorderAndPaddingLogicalLeft(),
      -layout_box.MarginBefore() - layout_box.BorderBefore() -
          layout_box.PaddingBefore());
  LayoutSize margin_box_size_delta(
      layout_box.MarginLogicalWidth() +
          layout_box.BorderAndPaddingLogicalWidth(),
      layout_box.MarginLogicalHeight() +
          layout_box.BorderAndPaddingLogicalHeight());
  LayoutSize margin_rect_size(reference_box_logical_size +
                              margin_box_size_delta);
  margin_rect_size.ClampNegativeToZero();
  return LayoutRect(margin_box_origin, margin_rect_size);
}

std::unique_ptr<Shape> ShapeOutsideInfo::CreateShapeForImage(
    StyleImage* style_image,
    float shape_image_threshold,
    WritingMode writing_mode,
    float margin) const {
  DCHECK(!style_image->IsPendingImage());

  RespectImageOrientationEnum respect_orientation =
      style_image->ForceOrientationIfNecessary(
          LayoutObject::ShouldRespectImageOrientation(layout_box_));

  const LayoutSize& image_size = RoundedLayoutSize(style_image->ImageSize(
      layout_box_->StyleRef().EffectiveZoom(),
      gfx::SizeF(reference_box_logical_size_), respect_orientation));

  const LayoutRect& margin_rect =
      GetShapeImageMarginRect(*layout_box_, reference_box_logical_size_);
  const LayoutRect& image_rect = (layout_box_->IsLayoutImage())
                                     ? To<LayoutImage>(layout_box_.Get())
                                           ->ReplacedContentRect()
                                           .ToLayoutRect()
                                     : LayoutRect(LayoutPoint(), image_size);

  scoped_refptr<Image> image =
      style_image->GetImage(*layout_box_, layout_box_->GetDocument(),
                            layout_box_->StyleRef(), gfx::SizeF(image_size));

  return Shape::CreateRasterShape(image.get(), shape_image_threshold,
                                  image_rect, margin_rect, writing_mode, margin,
                                  respect_orientation);
}

const Shape& ShapeOutsideInfo::ComputedShape() const {
  if (Shape* shape = shape_.get())
    return *shape;

  base::AutoReset<bool> is_in_computing_shape(&is_computing_shape_, true);

  const ComputedStyle& style = *layout_box_->Style();
  DCHECK(layout_box_->ContainingBlock());
  const LayoutBlock& containing_block = *layout_box_->ContainingBlock();
  const ComputedStyle& containing_block_style = containing_block.StyleRef();

  WritingMode writing_mode = containing_block_style.GetWritingMode();
  // Make sure contentWidth is not negative. This can happen when containing
  // block has a vertical scrollbar and its content is smaller than the
  // scrollbar width.
  LayoutUnit percentage_resolution_inline_size =
      containing_block.IsLayoutNGObject()
          ? percentage_resolution_inline_size_
          : std::max(LayoutUnit(), containing_block.ContentWidth());

  float margin =
      FloatValueForLength(layout_box_->StyleRef().ShapeMargin(),
                          percentage_resolution_inline_size.ToFloat());

  float shape_image_threshold = style.ShapeImageThreshold();
  DCHECK(style.ShapeOutside());
  const ShapeValue& shape_value = *style.ShapeOutside();

  switch (shape_value.GetType()) {
    case ShapeValue::kShape:
      DCHECK(shape_value.Shape());
      shape_ =
          Shape::CreateShape(shape_value.Shape(), reference_box_logical_size_,
                             writing_mode, margin);
      break;
    case ShapeValue::kImage:
      DCHECK(shape_value.GetImage());
      DCHECK(shape_value.GetImage()->IsLoaded());
      DCHECK(shape_value.GetImage()->CanRender());
      shape_ = CreateShapeForImage(shape_value.GetImage(),
                                   shape_image_threshold, writing_mode, margin);
      break;
    case ShapeValue::kBox: {
      // TODO(layout-dev): It seems incorrect to pass logical size to
      // RoundedBorderGeometry().
      const FloatRoundedRect& shape_rect = RoundedBorderGeometry::RoundedBorder(
          style, PhysicalRect(PhysicalOffset(), reference_box_logical_size_));
      shape_ = Shape::CreateLayoutBoxShape(shape_rect, writing_mode, margin);
      break;
    }
  }

  DCHECK(shape_);
  return *shape_;
}

inline LayoutUnit BorderBeforeInWritingMode(const LayoutBox& layout_box,
                                            WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return LayoutUnit(layout_box.BorderTop());
    case WritingMode::kVerticalLr:
      return LayoutUnit(layout_box.BorderLeft());
    case WritingMode::kVerticalRl:
      return LayoutUnit(layout_box.BorderRight());
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }

  NOTREACHED();
  return LayoutUnit(layout_box.BorderBefore());
}

inline LayoutUnit BorderAndPaddingBeforeInWritingMode(
    const LayoutBox& layout_box,
    WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return layout_box.BorderTop() + layout_box.PaddingTop();
    case WritingMode::kVerticalLr:
      return layout_box.BorderLeft() + layout_box.PaddingLeft();
    case WritingMode::kVerticalRl:
      return layout_box.BorderRight() + layout_box.PaddingRight();
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }

  NOTREACHED();
  return layout_box.BorderAndPaddingBefore();
}

LayoutUnit ShapeOutsideInfo::LogicalTopOffset() const {
  switch (ReferenceBox(*layout_box_->StyleRef().ShapeOutside())) {
    case CSSBoxType::kMargin:
      return -layout_box_->MarginBefore(
          layout_box_->ContainingBlock()->Style());
    case CSSBoxType::kBorder:
      return LayoutUnit();
    case CSSBoxType::kPadding:
      return BorderBeforeInWritingMode(
          *layout_box_,
          layout_box_->ContainingBlock()->StyleRef().GetWritingMode());
    case CSSBoxType::kContent:
      return BorderAndPaddingBeforeInWritingMode(
          *layout_box_,
          layout_box_->ContainingBlock()->StyleRef().GetWritingMode());
    case CSSBoxType::kMissing:
      break;
  }

  NOTREACHED();
  return LayoutUnit();
}

inline LayoutUnit BorderStartWithStyleForWritingMode(
    const LayoutBox& layout_box,
    const ComputedStyle* style) {
  if (style->IsHorizontalWritingMode()) {
    if (style->IsLeftToRightDirection())
      return LayoutUnit(layout_box.BorderLeft());

    return LayoutUnit(layout_box.BorderRight());
  }
  if (style->IsLeftToRightDirection())
    return LayoutUnit(layout_box.BorderTop());

  return LayoutUnit(layout_box.BorderBottom());
}

inline LayoutUnit BorderAndPaddingStartWithStyleForWritingMode(
    const LayoutBox& layout_box,
    const ComputedStyle* style) {
  if (style->IsHorizontalWritingMode()) {
    if (style->IsLeftToRightDirection())
      return layout_box.BorderLeft() + layout_box.PaddingLeft();

    return layout_box.BorderRight() + layout_box.PaddingRight();
  }
  if (style->IsLeftToRightDirection())
    return layout_box.BorderTop() + layout_box.PaddingTop();

  return layout_box.BorderBottom() + layout_box.PaddingBottom();
}

LayoutUnit ShapeOutsideInfo::LogicalLeftOffset() const {
  switch (ReferenceBox(*layout_box_->StyleRef().ShapeOutside())) {
    case CSSBoxType::kMargin:
      return -layout_box_->MarginStart(layout_box_->ContainingBlock()->Style());
    case CSSBoxType::kBorder:
      return LayoutUnit();
    case CSSBoxType::kPadding:
      return BorderStartWithStyleForWritingMode(
          *layout_box_, layout_box_->ContainingBlock()->Style());
    case CSSBoxType::kContent:
      return BorderAndPaddingStartWithStyleForWritingMode(
          *layout_box_, layout_box_->ContainingBlock()->Style());
    case CSSBoxType::kMissing:
      break;
  }

  NOTREACHED();
  return LayoutUnit();
}

bool ShapeOutsideInfo::IsEnabledFor(const LayoutBox& box) {
  ShapeValue* shape_value = box.StyleRef().ShapeOutside();
  if (!box.IsFloating() || !shape_value)
    return false;

  switch (shape_value->GetType()) {
    case ShapeValue::kShape:
      return shape_value->Shape();
    case ShapeValue::kImage: {
      StyleImage* image = shape_value->GetImage();
      DCHECK(image);
      return image->IsLoaded() && image->CanRender() &&
             CheckShapeImageOrigin(box.GetDocument(), *image);
    }
    case ShapeValue::kBox:
      return true;
  }

  return false;
}

PhysicalRect ShapeOutsideInfo::ComputedShapePhysicalBoundingBox() const {
  LayoutRect physical_bounding_box =
      ComputedShape().ShapeMarginLogicalBoundingBox();
  physical_bounding_box.SetX(physical_bounding_box.X() + LogicalLeftOffset());

  if (layout_box_->StyleRef().IsFlippedBlocksWritingMode())
    physical_bounding_box.SetY(layout_box_->LogicalHeight() -
                               physical_bounding_box.MaxY());
  else
    physical_bounding_box.SetY(physical_bounding_box.Y() + LogicalTopOffset());

  if (!layout_box_->StyleRef().IsHorizontalWritingMode())
    physical_bounding_box = physical_bounding_box.TransposedRect();
  else
    physical_bounding_box.SetY(physical_bounding_box.Y() + LogicalTopOffset());

  return PhysicalRect(physical_bounding_box);
}

gfx::PointF ShapeOutsideInfo::ShapeToLayoutObjectPoint(
    gfx::PointF point) const {
  gfx::PointF result = gfx::PointF(point.x() + LogicalLeftOffset(),
                                   point.y() + LogicalTopOffset());
  if (layout_box_->StyleRef().IsFlippedBlocksWritingMode())
    result.set_y(layout_box_->LogicalHeight() - result.y());
  if (!layout_box_->StyleRef().IsHorizontalWritingMode())
    result.Transpose();
  return result;
}

// static
ShapeOutsideInfo::InfoMap& ShapeOutsideInfo::GetInfoMap() {
  DEFINE_STATIC_LOCAL(Persistent<InfoMap>, static_info_map,
                      (MakeGarbageCollected<InfoMap>()));
  return *static_info_map;
}

void ShapeOutsideInfo::Trace(Visitor* visitor) const {
  visitor->Trace(layout_box_);
}

}  // namespace blink
