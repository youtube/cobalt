/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_H_

#include "third_party/blink/renderer/core/layout/layout_inline.h"

namespace blink {

class InlineCursor;

class LayoutSVGInline : public LayoutInline {
 public:
  explicit LayoutSVGInline(Element*);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGInline";
  }
  PaintLayerType LayerTypeRequired() const final {
    NOT_DESTROYED();
    return kNoPaintLayer;
  }
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVG || type == kLayoutObjectSVGInline ||
           LayoutInline::IsOfType(type);
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  gfx::RectF ObjectBoundingBox() const final;
  gfx::RectF DecoratedBoundingBox() const final;
  gfx::RectF VisualRectInLocalSVGCoordinates() const final;

  PhysicalRect VisualRectInDocument(
      VisualRectFlags = kDefaultVisualRectFlags) const final;
  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags) const final;
  void AbsoluteQuads(Vector<gfx::QuadF>&,
                     MapCoordinatesFlags mode = 0) const final;
  void AddOutlineRects(OutlineRectCollector&,
                       OutlineInfo*,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;

 private:
  void WillBeDestroyed() final;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) final;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) final;
  void RemoveChild(LayoutObject*) final;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  bool IsObjectBoundingBoxValid() const;

  static void ObjectBoundingBoxForCursor(InlineCursor& cursor,
                                         gfx::RectF& bounds);
};

template <>
struct DowncastTraits<LayoutSVGInline> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGInline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_INLINE_H_
