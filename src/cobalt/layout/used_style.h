// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_LAYOUT_USED_STYLE_H_
#define COBALT_LAYOUT_USED_STYLE_H_

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/radial_gradient_value.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/transform_matrix.h"
#include "cobalt/dom/font_cache.h"
#include "cobalt/dom/font_list.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/mesh/mesh_projection.h"
#include "cobalt/math/size.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/rounded_corners.h"

namespace cobalt {
namespace layout {

// The used value is the result of taking the computed value and completing any
// remaining calculations to make it the absolute theoretical value used in
// the layout of the document. If the property does not apply to this element,
// then the element has no used value for that property.
//   https://www.w3.org/TR/css-cascade-3/#used

// All used values are expressed in terms of render tree.
//
// This file contains facilities to convert CSSOM values to render tree values
// for properties not affected by the layout, such as "color" or "font-size",
// as for them the used values are the same as the computed values.

class ContainingBlock;

class UsedStyleProvider {
 public:
  // A function that will attach a camera matrix projection transform to a given
  // sub render tree.  |max_horizontal_fov_rad| and |max_vertical_fov_rad|
  // dictate the minimum field of view to use in that direction, in radians.
  // These minimum FOV values are compared against the values derived from
  // applying the aspect ratio to the other FOV value, and using whichever one
  // is most minimum.
  typedef base::Callback<scoped_refptr<render_tree::Node>(
      const scoped_refptr<render_tree::Node>&, float max_horizontal_fov_rad,
      float max_vertical_fov_rad)> AttachCameraNodeFunction;

  UsedStyleProvider(dom::HTMLElementContext* html_element_context,
                    dom::FontCache* font_cache,
                    const AttachCameraNodeFunction& attach_camera_node_function,
                    bool enable_image_animations);

  scoped_refptr<dom::FontList> GetUsedFontList(
      const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_style_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_weight_refptr);

  scoped_refptr<loader::image::Image> ResolveURLToImage(const GURL& url);
  scoped_refptr<loader::mesh::MeshProjection> ResolveURLToMeshProjection(
      const GURL& url);

  const AttachCameraNodeFunction attach_camera_node_function() const {
    return attach_camera_node_function_;
  }

  // Notifies animated image tracker to update the playing status of animated
  // images.
  void UpdateAnimatedImages();

  bool enable_image_animations() const { return enable_image_animations_; }

 private:
  // Called after layout is completed so that it can perform any necessary
  // cleanup. Its primary current purpose is making a request to the font cache
  // to remove any font lists that are no longer being referenced by boxes.
  void CleanupAfterLayout();

  dom::FontCache* const font_cache_;
  loader::image::AnimatedImageTracker* const animated_image_tracker_;
  loader::image::ImageCache* const image_cache_;
  loader::mesh::MeshCache* const mesh_cache_;
  AttachCameraNodeFunction attach_camera_node_function_;

  // |font_list_key_| is retained in between lookups so that the font names
  // vector will not need to allocate elements each time that it is populated.
  dom::FontListKey font_list_key_;

  // The last_font member variables are used to speed up |GetUsedFontList()|.
  // Around 85% of the time in current clients, the current font list matches
  // the last font list, so immediately comparing the current font list's
  // properties against the last font list's properties, prior to updating the
  // font list key and performing a font cache lookup, results in a significant
  // performance improvement.
  scoped_refptr<cssom::PropertyValue> last_font_family_refptr_;
  scoped_refptr<cssom::PropertyValue> last_font_style_refptr_;
  scoped_refptr<cssom::PropertyValue> last_font_weight_refptr_;
  scoped_refptr<dom::FontList> last_font_list_;

  // If true, animated WebP images should animate, otherwise they should not.
  const bool enable_image_animations_;

  friend class UsedStyleProviderLayoutScope;
  DISALLOW_COPY_AND_ASSIGN(UsedStyleProvider);
};

class UsedStyleProviderLayoutScope {
 public:
  explicit UsedStyleProviderLayoutScope(UsedStyleProvider* used_style_provider);
  ~UsedStyleProviderLayoutScope();

 private:
  UsedStyleProvider* const used_style_provider_;
};

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr);

LayoutUnit GetUsedLength(
    const scoped_refptr<cssom::PropertyValue>& length_refptr);

LayoutUnit GetUsedNonNegativeLength(
    const scoped_refptr<cssom::PropertyValue>& length_refptr);

class UsedBackgroundNodeProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundNodeProvider(
      const math::RectF& frame,
      const scoped_refptr<cssom::PropertyValue>& background_size,
      const scoped_refptr<cssom::PropertyValue>& background_position,
      const scoped_refptr<cssom::PropertyValue>& background_repeat,
      UsedStyleProvider* used_style_provider);

  void VisitAbsoluteURL(cssom::AbsoluteURLValue* url_value) override;
  void VisitLinearGradient(
      cssom::LinearGradientValue* linear_gradient_value) override;
  void VisitRadialGradient(
      cssom::RadialGradientValue* radial_gradient_value) override;

  scoped_refptr<render_tree::Node> background_node() {
    return background_node_;
  }

  bool is_opaque() const { return is_opaque_; }

 private:
  const math::RectF frame_;
  const scoped_refptr<cssom::PropertyValue> background_size_;
  const scoped_refptr<cssom::PropertyValue> background_position_;
  const scoped_refptr<cssom::PropertyValue> background_repeat_;
  UsedStyleProvider* const used_style_provider_;

  scoped_refptr<render_tree::Node> background_node_;

  bool is_opaque_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundNodeProvider);
};

class UsedBackgroundPositionProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundPositionProvider(const math::SizeF& frame_size,
                                 const math::SizeF& image_actual_size);

  void VisitPropertyList(
      cssom::PropertyListValue* property_list_value) override;

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
      cssom::PropertyListValue* property_list_value) override;

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
      cssom::PropertyListValue* property_list_value) override;
  void VisitKeyword(cssom::KeywordValue* keyword) override;

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

class UsedBorderRadiusProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedBorderRadiusProvider(const math::SizeF& frame_size);

  void VisitLength(cssom::LengthValue* length) override;
  void VisitPercentage(cssom::PercentageValue* percentage) override;

  const base::optional<render_tree::RoundedCorner>& rounded_corner() const {
    return rounded_corner_;
  }

 private:
  base::optional<render_tree::RoundedCorner> rounded_corner_;

  const math::SizeF frame_size_;

  DISALLOW_COPY_AND_ASSIGN(UsedBorderRadiusProvider);
};

class UsedLineHeightProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedLineHeightProvider(
      const render_tree::FontMetrics& font_metrics,
      const scoped_refptr<cssom::PropertyValue>& font_size);

  void VisitKeyword(cssom::KeywordValue* keyword) override;
  void VisitLength(cssom::LengthValue* length) override;
  void VisitNumber(cssom::NumberValue* length) override;

  LayoutUnit used_line_height() const { return used_line_height_; }
  LayoutUnit half_leading() const { return half_leading_; }

  // Half the leading is added above ascent (A) and the other half below
  // descent (D), giving the glyph and its leading (L) a total height above
  // the baseline of A' = A + L/2 and a total depth of D' = D + L/2.
  //   https://www.w3.org/TR/CSS21/visudet.html#leading
  LayoutUnit baseline_offset_from_top() const {
    return LayoutUnit(LayoutUnit(font_metrics_.ascent()) + half_leading_);
  }
  LayoutUnit baseline_offset_from_bottom() const {
    return LayoutUnit(LayoutUnit(font_metrics_.descent()) + half_leading_);
  }

 private:
  void UpdateHalfLeading();

  const render_tree::FontMetrics font_metrics_;
  const scoped_refptr<cssom::PropertyValue> font_size_;

  LayoutUnit used_line_height_;
  LayoutUnit half_leading_;

  DISALLOW_COPY_AND_ASSIGN(UsedLineHeightProvider);
};

math::Vector2dF GetTransformOrigin(const math::RectF& used_rect,
                                   cssom::PropertyValue* value);

cssom::TransformMatrix GetTransformMatrix(cssom::PropertyValue* value);

// Functions to calculate used values of box model properties.
base::optional<LayoutUnit> GetUsedLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedTopIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedRightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedWidthIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block);
base::optional<LayoutUnit> GetUsedMaxHeightIfNotNone(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* height_depends_on_containing_block);
base::optional<LayoutUnit> GetUsedMaxWidthIfNotNone(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block);
LayoutUnit GetUsedMinHeight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* height_depends_on_containing_block);
LayoutUnit GetUsedMinWidth(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block);
base::optional<LayoutUnit> GetUsedHeightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedMarginLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedMarginTopIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedMarginRightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
base::optional<LayoutUnit> GetUsedMarginBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
LayoutUnit GetUsedBorderLeft(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style);
LayoutUnit GetUsedBorderTop(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style);
LayoutUnit GetUsedBorderRight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style);
LayoutUnit GetUsedBorderBottom(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style);
LayoutUnit GetUsedPaddingLeft(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
LayoutUnit GetUsedPaddingTop(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
LayoutUnit GetUsedPaddingRight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);
LayoutUnit GetUsedPaddingBottom(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size);

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_USED_STYLE_H_
