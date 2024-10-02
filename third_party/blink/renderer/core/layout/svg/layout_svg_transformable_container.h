/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_TRANSFORMABLE_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_TRANSFORMABLE_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class SVGGraphicsElement;

class LayoutSVGTransformableContainer final : public LayoutSVGContainer {
 public:
  explicit LayoutSVGTransformableContainer(SVGGraphicsElement*);

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVGTransformableContainer ||
           LayoutSVGContainer::IsOfType(type);
  }
  const gfx::Vector2dF& AdditionalTranslation() const {
    NOT_DESTROYED();
    return additional_translation_;
  }

  void SetNeedsTransformUpdate() override;

 private:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  SVGTransformChange CalculateLocalTransform(bool bounds_changed) override;
  AffineTransform LocalSVGTransform() const override {
    NOT_DESTROYED();
    return local_transform_;
  }

  bool needs_transform_update_ : 1;
  bool transform_uses_reference_box_ : 1;
  AffineTransform local_transform_;
  gfx::Vector2dF additional_translation_;
};

template <>
struct DowncastTraits<LayoutSVGTransformableContainer> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGTransformableContainer();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_TRANSFORMABLE_CONTAINER_H_
