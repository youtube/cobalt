/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_clip_path_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGClipPathElement::SVGClipPathElement(Document& document)
    : SVGGraphicsElement(svg_names::kClipPathTag, document),
      clip_path_units_(MakeGarbageCollected<
                       SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kClipPathUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  AddToPropertyMap(clip_path_units_);
}

void SVGClipPathElement::Trace(Visitor* visitor) const {
  visitor->Trace(clip_path_units_);
  SVGGraphicsElement::Trace(visitor);
}

void SVGClipPathElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (params.name == svg_names::kClipPathUnitsAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject());
    if (layout_object)
      layout_object->InvalidateCacheAndMarkForLayout();
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
}

void SVGClipPathElement::ChildrenChanged(const ChildrenChange& change) {
  SVGGraphicsElement::ChildrenChanged(change);

  if (change.ByParser())
    return;

  if (LayoutObject* object = GetLayoutObject()) {
    object->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kChildChanged);
  }
}

LayoutObject* SVGClipPathElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGResourceClipper>(this);
}

}  // namespace blink
