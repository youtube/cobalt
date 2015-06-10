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
#include "cobalt/loader/image_cache.h"
#include "cobalt/math/size.h"
#include "cobalt/math/size_f.h"
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
  UsedStyleProvider(render_tree::ResourceProvider* resource_provider,
                    loader::ImageCache* image_cache);

  scoped_refptr<render_tree::Font> GetUsedFont(
      const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
      const scoped_refptr<cssom::PropertyValue>& font_weight_refptr) const;

  scoped_refptr<render_tree::Image> GetUsedBackgroundImage(
      const scoped_refptr<cssom::PropertyValue>& background_image_refptr) const;

 private:
  render_tree::ResourceProvider* const resource_provider_;
  loader::ImageCache* const image_cache_;

  DISALLOW_COPY_AND_ASSIGN(UsedStyleProvider);
};

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr);

class UsedHeightProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedHeightProvider(float containing_block_height)
      : containing_block_height_(containing_block_height) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;
  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE;

  float used_height() const { return used_height_; }

 protected:
  virtual void VisitAuto() = 0;

  void set_used_height(float used_height) { used_height_ = used_height; }

 private:
  const float containing_block_height_;

  float used_height_;

  DISALLOW_COPY_AND_ASSIGN(UsedHeightProvider);
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


class UsedWidthProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedWidthProvider(float containing_block_width)
      : containing_block_width_(containing_block_width) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;
  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE;

  float used_width() const { return used_width_; }

  bool width_depends_on_containing_block() const {
    return width_depends_on_containing_block_;
  }

 protected:
  virtual void VisitAuto() = 0;

  float containing_block_width() const { return containing_block_width_; }

  void set_used_width(float used_width) { used_width_ = used_width; }

 private:
  const float containing_block_width_;

  float used_width_;
  bool width_depends_on_containing_block_;

  DISALLOW_COPY_AND_ASSIGN(UsedWidthProvider);
};

class UsedPositionOffsetProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedPositionOffsetProvider(float containing_block_size);

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;
  void VisitLength(cssom::LengthValue* length) OVERRIDE;
  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE;

  float used_position_offset() const { return used_position_offset_; }
  bool is_auto() const { return is_auto_; }

 private:
  const float containing_block_size_;

  float used_position_offset_;
  bool is_auto_;

  DISALLOW_COPY_AND_ASSIGN(UsedPositionOffsetProvider);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_USED_STYLE_H_
