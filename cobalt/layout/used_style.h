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

#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/loader/image_cache.h"
#include "cobalt/math/size.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/node.h"
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
  UsedStyleProvider(render_tree::ResourceProvider* resource_provider,
                    loader::ImageCache* image_cache);

  scoped_refptr<render_tree::Font> GetUsedFont(
      const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_weight_refptr) const;

  scoped_refptr<render_tree::Image> ResolveURLToImage(const GURL& url) const;

 private:
  render_tree::ResourceProvider* const resource_provider_;
  loader::ImageCache* const image_cache_;

  DISALLOW_COPY_AND_ASSIGN(UsedStyleProvider);
};

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr);

class UsedBackgroundNodeProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundNodeProvider(
      const UsedStyleProvider* used_style_provider,
      const math::SizeF& frame_size,
      const scoped_refptr<cssom::PropertyValue>& background_size,
      const scoped_refptr<cssom::PropertyValue>& background_position);

  void VisitAbsoluteURL(cssom::AbsoluteURLValue* url_value) OVERRIDE;

  scoped_refptr<render_tree::Node> background_node() {
    return background_node_;
  }

 private:
  const UsedStyleProvider* const used_style_provider_;
  const math::SizeF frame_size_;
  const scoped_refptr<cssom::PropertyValue> background_size_;
  const scoped_refptr<cssom::PropertyValue> background_position_;

  scoped_refptr<render_tree::Node> background_node_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundNodeProvider);
};

class UsedBackgroundPositionProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundPositionProvider(const math::SizeF& frame_size,
                                 const math::SizeF& image_actual_size);

  void VisitPropertyList(
      cssom::PropertyListValue* property_list_value) OVERRIDE;

  float translate_x() { return translate_x_; }
  float translate_y() { return translate_y_; }

 private:
  const math::SizeF frame_size_;
  const math::SizeF image_actual_size_;

  float translate_x_;
  float translate_y_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundPositionProvider);
};

class UsedBackgroundSizeProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundSizeProvider(const math::SizeF& frame_size,
                             const math::Size& image_size);

  void VisitPropertyList(
      cssom::PropertyListValue* property_list_value) OVERRIDE;
  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  float width_scale() const { return width_scale_; }
  float height_scale() const { return height_scale_; }

  float width() const { return width_scale_ * frame_size_.width(); }
  float height() const { return height_scale_ * frame_size_.height(); }

 private:
  void ConvertWidthAndHeightScale(float width_scale, float height_scale);

  const math::SizeF frame_size_;
  const math::Size image_size_;

  float width_scale_;
  float height_scale_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundSizeProvider);
};

class UsedLineHeightProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedLineHeightProvider(const render_tree::FontMetrics& font_metrics);

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;

  float used_line_height() const { return used_line_height_; }

 private:
  const render_tree::FontMetrics font_metrics_;

  float used_line_height_;

  DISALLOW_COPY_AND_ASSIGN(UsedLineHeightProvider);
};

// This class should be used when one needs to determine any of the margin,
// padding, position (e.g. 'left'/'top') or content size properties.  Since much
// functionality is common between width and height, the same code can typically
// be re-used for each, and thus the UsedBoxMetrics class itself has general
// names (e.g. "size" instead of "width" or "height",).
class UsedBoxMetrics {
 public:
  // Constructs and returns a UsedBoxMetrics object based on the horizontal
  // properties set on the passed in computed_style.  Horizontal here refers
  // to left/right/width/etc...
  static UsedBoxMetrics ComputeHorizontal(
      float containing_block_size,
      const cssom::CSSStyleDeclarationData& computed_style) {
    return ComputeMetrics(containing_block_size, computed_style.position(),
                          computed_style.left(), computed_style.width(),
                          computed_style.right());
  }

  // Constructs and returns a UsedBoxMetrics object based on the vertical
  // properties set on the passed in computed_style.  Vertical here refers
  // to top/bottom/height/etc...
  static UsedBoxMetrics ComputeVertical(
      float containing_block_size,
      const cssom::CSSStyleDeclarationData& computed_style) {
    return ComputeMetrics(containing_block_size, computed_style.position(),
                          computed_style.top(), computed_style.height(),
                          computed_style.bottom());
  }

  // The following constraints must hold among the used values of the
  // properties:
  //     margin-left + border-left-width + padding-left
  //   + width
  //   + padding-right + border-right-width + margin-right
  //   = width of containing block
  // (And similarly for heights)
  //
  // This function will solve for the variables as much as possible given
  // values that are already filled in.
  void ResolveConstraints(float containing_block_size);

  // If a value is available, it is a length with pixel units.
  base::optional<float> start_offset;
  base::optional<float> size;
  base::optional<float> end_offset;

  bool size_depends_on_containing_block;
  bool offset_depends_on_containing_block;

  // TODO(***REMOVED***): Add support for padding and margins.

 private:
  // Used internally by ComputeHorizontal() and ComputeVertical(), after they
  // have selected the appropritate properties from the computed style to pass
  // to this method.
  static UsedBoxMetrics ComputeMetrics(float containing_block_size,
                                       cssom::PropertyValue* position,
                                       cssom::PropertyValue* start_offset,
                                       cssom::PropertyValue* size,
                                       cssom::PropertyValue* end_offset);
};
}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_USED_STYLE_H_
