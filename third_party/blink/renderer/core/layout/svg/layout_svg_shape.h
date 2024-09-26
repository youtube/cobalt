/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Computer, Inc
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_SHAPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_SHAPE_H_

#include <memory>
#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_marker_data.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class PointerEventsHitRules;
class SVGGeometryElement;

enum ShapeGeometryCodePath {
  kPathGeometry,
  kRectGeometryFastPath,
  kEllipseGeometryFastPath
};

struct LayoutSVGShapeRareData {
  USING_FAST_MALLOC(LayoutSVGShapeRareData);

 public:
  LayoutSVGShapeRareData() = default;
  LayoutSVGShapeRareData(const LayoutSVGShapeRareData&) = delete;
  LayoutSVGShapeRareData& operator=(const LayoutSVGShapeRareData) = delete;
  Path non_scaling_stroke_path_;
  AffineTransform non_scaling_stroke_transform_;
};

class LayoutSVGShape : public LayoutSVGModelObject {
 public:
  ~LayoutSVGShape() override;

  void SetNeedsShapeUpdate() {
    NOT_DESTROYED();
    needs_shape_update_ = true;
  }
  void SetNeedsBoundariesUpdate() final {
    NOT_DESTROYED();
    needs_boundaries_update_ = true;
  }
  void SetNeedsTransformUpdate() final {
    NOT_DESTROYED();
    needs_transform_update_ = true;
  }

  Path& GetPath() const {
    NOT_DESTROYED();
    DCHECK(path_);
    return *path_;
  }
  bool HasPath() const {
    NOT_DESTROYED();
    return path_.get();
  }
  float DashScaleFactor() const;

  // This method is sometimes (rarely) called with a null path and crashes. The
  // code managing the path enforces the necessary invariants to ensure a valid
  // path but somehow that fails. The assert and check for hasPath() are
  // intended to detect and prevent crashes.
  virtual bool IsShapeEmpty() const {
    NOT_DESTROYED();
    DCHECK(path_);
    return !HasPath() || GetPath().IsEmpty();
  }

  bool HasNonScalingStroke() const {
    NOT_DESTROYED();
    return StyleRef().VectorEffect() == EVectorEffect::kNonScalingStroke;
  }
  const Path& NonScalingStrokePath() const {
    NOT_DESTROYED();
    DCHECK(HasNonScalingStroke());
    DCHECK(rare_data_);
    return rare_data_->non_scaling_stroke_path_;
  }
  const AffineTransform& NonScalingStrokeTransform() const {
    NOT_DESTROYED();
    DCHECK(HasNonScalingStroke());
    DCHECK(rare_data_);
    return rare_data_->non_scaling_stroke_transform_;
  }

  AffineTransform ComputeRootTransform() const;
  AffineTransform ComputeNonScalingStrokeTransform() const;
  AffineTransform LocalSVGTransform() const final {
    NOT_DESTROYED();
    return local_transform_;
  }

  virtual const Vector<MarkerPosition>* MarkerPositions() const {
    NOT_DESTROYED();
    return nullptr;
  }

  float StrokeWidth() const;
  float StrokeWidthForMarkerUnits() const;

  virtual ShapeGeometryCodePath GeometryCodePath() const {
    NOT_DESTROYED();
    return kPathGeometry;
  }

  gfx::RectF ObjectBoundingBox() const final {
    NOT_DESTROYED();
    return fill_bounding_box_;
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGShape";
  }

 protected:
  // Description of the geometry of the shape for stroking.
  enum StrokeGeometryClass : uint8_t {
    kComplex,   // We don't know anything about the geometry => use the generic
                // approximation.
    kNoMiters,  // We know that the shape will not have any joins, so no miters
                // will be generated. This means we can use an approximation
                // that does not factor in miters (and thus get tighter
                // approximated bounds.)
    kSimple,    // We know that the geometry is convex and has no acute angles
                // (rect, rounded rect, circle, ellipse) => use the simple
                // approximation.
  };
  LayoutSVGShape(SVGGeometryElement*, StrokeGeometryClass);

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

  RasterEffectOutset VisualRectOutsetForRasterEffects() const override;

  void ClearPath();
  void CreatePath();

  // Update (cached) shape data and the (object) bounding box.
  virtual void UpdateShapeFromElement();
  gfx::RectF CalculateStrokeBoundingBox() const;
  virtual bool ShapeDependentStrokeContains(const HitTestLocation&);
  virtual bool ShapeDependentFillContains(const HitTestLocation&,
                                          const WindRule) const;

  gfx::RectF fill_bounding_box_;
  gfx::RectF stroke_bounding_box_;

  LayoutSVGShapeRareData& EnsureRareData() const;

 private:
  // Hit-detection separated for the fill and the stroke
  bool FillContains(const HitTestLocation&,
                    bool requires_fill = true,
                    const WindRule fill_rule = RULE_NONZERO);
  bool StrokeContains(const HitTestLocation&, bool requires_stroke = true);

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVGShape ||
           LayoutSVGModelObject::IsOfType(type);
  }
  void UpdateLayout() final;
  void Paint(const PaintInfo&) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestPhase) final;
  bool HitTestShape(const HitTestRequest&,
                    const HitTestLocation&,
                    PointerEventsHitRules);

  gfx::RectF StrokeBoundingBox() const final {
    NOT_DESTROYED();
    return stroke_bounding_box_;
  }

  // Calculates an inclusive bounding box of this shape as if this shape has a
  // stroke. If this shape has a stroke, then |stroke_bounding_box_| is
  // returned; otherwise, estimates a bounding box (not necessarily tight) that
  // would include this shape's stroke bounding box if it had a stroke.
  gfx::RectF HitTestStrokeBoundingBox() const;
  // Compute an approximation of the bounding box that this stroke geometry
  // would generate when applied to the shape.
  gfx::RectF ApproximateStrokeBoundingBox(const gfx::RectF& shape_bounds) const;
  gfx::RectF CalculateNonScalingStrokeBoundingBox() const;
  void UpdateNonScalingStrokeData();

 private:
  AffineTransform local_transform_;
  // TODO(fmalita): the Path is now cached in SVGPath; while this additional
  // cache is just a shallow copy, it certainly has a complexity/state
  // management cost (plus allocation & storage overhead) - so we should look
  // into removing it.
  std::unique_ptr<Path> path_;
  mutable std::unique_ptr<LayoutSVGShapeRareData> rare_data_;
  std::unique_ptr<Path> stroke_path_cache_;

  StrokeGeometryClass geometry_class_;
  bool needs_boundaries_update_ : 1;
  bool needs_shape_update_ : 1;
  bool needs_transform_update_ : 1;
  bool transform_uses_reference_box_ : 1;
};

template <>
struct DowncastTraits<LayoutSVGShape> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGShape();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_SHAPE_H_
