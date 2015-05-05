/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LAYOUT_USED_STYLE_H_
#define LAYOUT_USED_STYLE_H_

#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace layout {

// The used value is the result of taking the computed value and completing any
// remaining calculations to make it the absolute theoretical value used in
// the layout of the document. If the property does not apply to this element,
// then the element has no used value for that property.
//   http://www.w3.org/TR/css-cascade-3/#used

// All used values are expressed in terms of render tree.
//
// This file contains facilities to convert CSSOM values to render tree values
// for properties not affected by the layout, such as "color" or "font-size",
// as for them the used values are the same as the computed values.
//
// Used values for the properties affected by the layout are converted from
// computed values in the Layout() methods of the Box subclasses.

class ContainingBlock;

class UsedStyleProvider {
 public:
  explicit UsedStyleProvider(render_tree::ResourceProvider* resource_provider);

  scoped_refptr<render_tree::Font> GetUsedFont(
      const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_size_refptr) const;

 private:
  render_tree::ResourceProvider* const resource_provider_;

  DISALLOW_COPY_AND_ASSIGN(UsedStyleProvider);
};

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr);

class UsedHeightProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedHeightProvider(float total_child_height);

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;

  float used_height() const { return used_height_; }

 private:
  float const total_child_height_;
  float used_height_;

  DISALLOW_COPY_AND_ASSIGN(UsedHeightProvider);
};

class UsedWidthProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedWidthProvider(float total_child_width);

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;

  float used_width() const { return used_width_; }

 private:
  float const total_child_width_;
  float used_width_;

  DISALLOW_COPY_AND_ASSIGN(UsedWidthProvider);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_USED_STYLE_H_
