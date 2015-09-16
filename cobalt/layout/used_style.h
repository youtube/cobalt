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
#include "cobalt/cssom/transform_function_list_value.h"
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

class ContainingBlock;

class UsedStyleProvider {
 public:
  UsedStyleProvider(render_tree::ResourceProvider* resource_provider,
                    loader::ImageCache* image_cache);

  scoped_refptr<render_tree::Font> GetUsedFont(
      const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_style_refptr,
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
      const scoped_refptr<cssom::PropertyValue>& background_position,
      const scoped_refptr<cssom::PropertyValue>& background_repeat);

  void VisitAbsoluteURL(cssom::AbsoluteURLValue* url_value) OVERRIDE;

  scoped_refptr<render_tree::Node> background_node() {
    return background_node_;
  }

 private:
  const UsedStyleProvider* const used_style_provider_;
  const math::SizeF frame_size_;
  const scoped_refptr<cssom::PropertyValue> background_size_;
  const scoped_refptr<cssom::PropertyValue> background_position_;
  const scoped_refptr<cssom::PropertyValue> background_repeat_;

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

  float translate_x_relative_to_frame() {
    return frame_size_.width() == 0.0f ? 0.0f
                                       : translate_x_ / frame_size_.width();
  }
  float translate_y_relative_to_frame() {
    return frame_size_.height() == 0.0f ? 0.0f
                                        : translate_y_ / frame_size_.height();
  }

 private:
  const math::SizeF frame_size_;
  const math::SizeF image_actual_size_;

  float translate_x_;
  float translate_y_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundPositionProvider);
};

class UsedBackgroundRepeatProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundRepeatProvider();

  void VisitPropertyList(
      cssom::PropertyListValue* property_list_value) OVERRIDE;

  bool repeat_x() { return repeat_x_; }
  bool repeat_y() { return repeat_y_; }

 private:
  bool repeat_x_;
  bool repeat_y_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundRepeatProvider);
};

class UsedBackgroundSizeProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundSizeProvider(const math::SizeF& frame_size,
                             const math::Size& image_size);

  void VisitPropertyList(
      cssom::PropertyListValue* property_list_value) OVERRIDE;
  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  float width_scale_relative_to_frame() const {
    return frame_size_.width() == 0.0f ? 0.0f : width_ / frame_size_.width();
  }
  float height_scale_relative_to_frame() const {
    return frame_size_.height() == 0.0f ? 0.0f : height_ / frame_size_.height();
  }

  float width() const { return width_; }
  float height() const { return height_; }

 private:
  void ConvertWidthAndHeightScale(float width_scale, float height_scale);

  const math::SizeF frame_size_;
  const math::Size image_size_;

  float width_;
  float height_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundSizeProvider);
};

class UsedLineHeightProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedLineHeightProvider(const render_tree::FontMetrics& font_metrics);

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;

  float used_line_height() const { return used_line_height_; }

  // Half the leading is added above ascent (A) and the other half below
  // descent (D), giving the glyph and its leading (L) a total height above
  // the baseline of A' = A + L/2 and a total depth of D' = D + L/2.
  //   http://www.w3.org/TR/CSS21/visudet.html#leading
  float baseline_offset_from_top() const {
    return font_metrics_.ascent + half_leading_;
  }
  float baseline_offset_from_bottom() const {
    return font_metrics_.descent + half_leading_;
  }

 private:
  void UpdateHalfLeading();

  const render_tree::FontMetrics font_metrics_;

  float used_line_height_;
  float half_leading_;

  DISALLOW_COPY_AND_ASSIGN(UsedLineHeightProvider);
};

scoped_refptr<cssom::TransformFunctionListValue> GetUsedTransformListValue(
    cssom::TransformFunctionListValue* transform_list,
    const math::SizeF& bounding_box);

// Functions to calculate used values of box model properties.
base::optional<float> GetUsedLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedTopIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedRightIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedWidthIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size,
    bool* width_depends_on_containing_block);
base::optional<float> GetUsedHeightIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedMarginLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedMarginTopIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedMarginRightIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
base::optional<float> GetUsedMarginBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
float GetUsedPaddingLeft(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
float GetUsedPaddingTop(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
float GetUsedPaddingRight(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);
float GetUsedPaddingBottom(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const math::SizeF& containing_block_size);

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_USED_STYLE_H_
