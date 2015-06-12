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

#include "cobalt/layout/used_style.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"

namespace cobalt {
namespace layout {

namespace {

render_tree::FontStyle ConvertFontWeightToRenderTreeFontStyle(
    cssom::FontWeightValue::Value value) {
  switch (value) {
    case cssom::FontWeightValue::kNormalAka400:
      return render_tree::kNormal;
    case cssom::FontWeightValue::kBoldAka700:
      return render_tree::kBold;
    case cssom::FontWeightValue::kThinAka100:
    case cssom::FontWeightValue::kExtraLightAka200:
    case cssom::FontWeightValue::kLightAka300:
    case cssom::FontWeightValue::kMediumAka500:
    case cssom::FontWeightValue::kSemiBoldAka600:
    case cssom::FontWeightValue::kExtraBoldAka800:
    case cssom::FontWeightValue::kBlackAka900:
    default:
      NOTREACHED() << "Not supported value: " << value;
      return render_tree::kNormal;
  }
}

class UsedBackgroundImageProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedBackgroundImageProvider(loader::ImageCache* image_cache)
      : image_cache_(image_cache) {}

  void VisitAbsoluteURL(cssom::AbsoluteURLValue* url_value) OVERRIDE;
  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  const scoped_refptr<render_tree::Image>& used_background_image() const {
    return used_background_image_;
  }

 private:
  loader::ImageCache* const image_cache_;
  scoped_refptr<render_tree::Image> used_background_image_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundImageProvider);
};

void UsedBackgroundImageProvider::VisitAbsoluteURL(
    cssom::AbsoluteURLValue* url_value) {
  used_background_image_ = image_cache_->TryGetImage(url_value->value());
}

void UsedBackgroundImageProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kNone:
      used_background_image_ = NULL;
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

class UsedBackgroundTranslateProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundTranslateProvider(float frame_length, float image_length)
      : frame_length_(frame_length), image_length_(image_length) {}

  void VisitLength(cssom::LengthValue* length) OVERRIDE;
  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE;

  float translate() { return translate_; }

 private:
  const float frame_length_;
  const float image_length_;

  float translate_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundTranslateProvider);
};

void UsedBackgroundTranslateProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  translate_ = frame_length_ == 0 ? 0 : length->value() / frame_length_;
}

// A percentage for the horizontal offset is relative to (width of background
// positioning area - width of background image). A percentage for the vertical
// offset is relative to (height of background positioning area - height of
// background image), where the size of the image is the size given by
// 'background-size'.
//   http://www.w3.org/TR/css3-background/#the-background-position
void UsedBackgroundTranslateProvider::VisitPercentage(
    cssom::PercentageValue* percentage) {
  translate_ = frame_length_ == 0 ? 0 : percentage->value() *
                                            (frame_length_ - image_length_) /
                                            frame_length_;
}

//   http://www.w3.org/TR/css3-background/#the-background-size
class UsedBackgroundSizeScaleProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundSizeScaleProvider(float frame_length, int image_length)
      : frame_length_(frame_length),
        image_length_(image_length),
        scale_(1.0f),
        auto_keyword_(false) {
    DCHECK_GT(image_length, 0);
  }

  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE;
  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  float scale() const { return scale_; }
  bool auto_keyword() const { return auto_keyword_; }

 private:
  const float frame_length_;
  const int image_length_;

  float scale_;
  bool auto_keyword_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundSizeScaleProvider);
};

void UsedBackgroundSizeScaleProvider::VisitPercentage(
    cssom::PercentageValue* percentage) {
  scale_ = frame_length_ * percentage->value() / image_length_;
}

void UsedBackgroundSizeScaleProvider::VisitKeyword(
    cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto: {
      auto_keyword_ = true;
      break;
    }
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

}  // namespace

UsedStyleProvider::UsedStyleProvider(
    render_tree::ResourceProvider* resource_provider,
    loader::ImageCache* image_cache)
    : resource_provider_(resource_provider), image_cache_(image_cache) {}

scoped_refptr<render_tree::Font> UsedStyleProvider::GetUsedFont(
    const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_weight_refptr) const {
  cssom::StringValue* font_family =
      base::polymorphic_downcast<cssom::StringValue*>(font_family_refptr.get());
  cssom::LengthValue* font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(font_size_refptr.get());
  DCHECK_EQ(cssom::kPixelsUnit, font_size->unit());
  cssom::FontWeightValue* font_weight =
      base::polymorphic_downcast<cssom::FontWeightValue*>(
          font_weight_refptr.get());
  render_tree::FontStyle font_style =
      ConvertFontWeightToRenderTreeFontStyle(font_weight->value());

  return resource_provider_->GetPreInstalledFont(
      font_family->value().c_str(), font_style, font_size->value());
}

scoped_refptr<render_tree::Image> UsedStyleProvider::GetUsedBackgroundImage(
    const scoped_refptr<cssom::PropertyValue>& background_image_refptr) const {
  UsedBackgroundImageProvider used_background_image_provider(image_cache_);
  background_image_refptr->Accept(&used_background_image_provider);
  return used_background_image_provider.used_background_image();
}

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr) {
  cssom::RGBAColorValue* color =
      base::polymorphic_downcast<cssom::RGBAColorValue*>(color_refptr.get());
  return render_tree::ColorRGBA(color->value());
}

//   http://www.w3.org/TR/css3-background/#the-background-position
UsedBackgroundPositionProvider::UsedBackgroundPositionProvider(
    const math::SizeF& frame_size, const math::SizeF& image_actual_size)
    : frame_size_(frame_size), image_actual_size_(image_actual_size) {}

void UsedBackgroundPositionProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list_value) {
  // TODO(***REMOVED***): Support more background-position other than percentage.
  DCHECK_EQ(property_list_value->value().size(), 2);
  UsedBackgroundTranslateProvider width_translate_provider(
      frame_size_.width(), image_actual_size_.width());
  property_list_value->value()[0]->Accept(&width_translate_provider);
  translate_x_ = width_translate_provider.translate();

  UsedBackgroundTranslateProvider height_translate_provider(
      frame_size_.height(), image_actual_size_.height());
  property_list_value->value()[1]->Accept(&height_translate_provider);
  translate_y_ = height_translate_provider.translate();
}

UsedBackgroundSizeProvider::UsedBackgroundSizeProvider(
    const math::SizeF& frame_size, const math::Size& image_size)
    : frame_size_(frame_size),
      image_size_(image_size),
      width_scale_(1.0f),
      height_scale_(1.0f) {}

// The first value gives the width of the corresponding image, and the second
// value gives its height.
//   http://www.w3.org/TR/css3-background/#the-background-size
void UsedBackgroundSizeProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list_value) {
  DCHECK_EQ(property_list_value->value().size(), 2);

  UsedBackgroundSizeScaleProvider used_background_width_provider(
      frame_size_.width(), image_size_.width());
  property_list_value->value()[0]->Accept(&used_background_width_provider);

  UsedBackgroundSizeScaleProvider used_background_height_provider(
      frame_size_.height(), image_size_.height());
  property_list_value->value()[1]->Accept(&used_background_height_provider);

  bool background_width_auto = used_background_width_provider.auto_keyword();
  bool background_height_auto = used_background_height_provider.auto_keyword();

  float width_scale;
  float height_scale;
  if (background_width_auto && background_height_auto) {
    // If both values are 'auto' then the intrinsic width and/or height of the
    // image should be used.
    width_scale = height_scale = 1.0f;
  } else if (!background_width_auto && !background_height_auto) {
    width_scale = used_background_width_provider.scale();
    height_scale = used_background_height_provider.scale();
  } else {
    // An 'auto' value for one dimension is resolved by using the image's
    // intrinsic ratio and the size of the other dimension.
    width_scale = height_scale = background_width_auto
                                     ? used_background_height_provider.scale()
                                     : used_background_width_provider.scale();
  }

  ConvertWidthAndHeightScale(width_scale, height_scale);
}

void UsedBackgroundSizeProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kContain: {
      // Scale the image, while preserving its intrinsic aspect ratio (if any),
      // to the largest size such that both its width and its height can
      // fit inside the background positioning area.
      float width_scale = frame_size_.width() / image_size_.width();
      float height_scale = frame_size_.height() / image_size_.height();

      float selected_scale =
          width_scale < height_scale ? width_scale : height_scale;
      ConvertWidthAndHeightScale(selected_scale, selected_scale);
      break;
    }
    case cssom::KeywordValue::kCover: {
      // Scale the image, while preserving its intrinsic aspect ratio (if any),
      // to the smallest size such that both its width and its height can
      // completely cover the background positioning area.
      float width_scale = frame_size_.width() / image_size_.width();
      float height_scale = frame_size_.height() / image_size_.height();

      float selected_scale =
          width_scale > height_scale ? width_scale : height_scale;
      ConvertWidthAndHeightScale(selected_scale, selected_scale);
      break;
    }
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void UsedBackgroundSizeProvider::ConvertWidthAndHeightScale(
    float width_scale, float height_scale) {
  if (frame_size_.width() <= 0 || frame_size_.height() <= 0) {
    DLOG(WARNING) << "Frame size is not positive.";
    width_scale_ = height_scale_ = 1.0f;
    return;
  }

  width_scale_ = width_scale * image_size_.width() / frame_size_.width();
  height_scale_ = height_scale * image_size_.height() / frame_size_.height();
}

UsedLineHeightProvider::UsedLineHeightProvider(
    const render_tree::FontMetrics& font_metrics)
    : font_metrics_(font_metrics) {}

void UsedLineHeightProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  DCHECK_EQ(cssom::KeywordValue::kNormal, keyword->value());
  used_line_height_ =
      font_metrics_.ascent + font_metrics_.descent + font_metrics_.leading;
}

void UsedLineHeightProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  used_line_height_ = length->value();
}

// UsedSizeMetricProvider is a visitor that is intended to visit property values
// for the properties "width", "height", "left", "right", "top", "bottom" as
// well as margin/border/padding sizes.  In all cases, we are dealing with at
// most absolute length values, percentage values, or the "auto" keyword.  This
// visitor will either forward the absolute length value, resolve the percentage
// value, or indicate that we recieved the "auto" value.  Since both length
// and percentage resolve to absolute length, we use an engaged base::optional
// to represent the length if a percentage or length was used, and an
// unengaged base::optional to represent that "auto" was used.
class UsedSizeMetricProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedSizeMetricProvider(float containing_block_size)
      : containing_block_size_(containing_block_size),
        depends_on_containing_block_(false) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE {
    switch (keyword->value()) {
      case cssom::KeywordValue::kAuto:
        // Nothing to do here, as we need to simply leave value_ in its
        // disengaged state to indicate that "auto" was the value.
        break;

      case cssom::KeywordValue::kAbsolute:
      case cssom::KeywordValue::kBlock:
      case cssom::KeywordValue::kContain:
      case cssom::KeywordValue::kCover:
      case cssom::KeywordValue::kHidden:
      case cssom::KeywordValue::kInherit:
      case cssom::KeywordValue::kInitial:
      case cssom::KeywordValue::kInline:
      case cssom::KeywordValue::kInlineBlock:
      case cssom::KeywordValue::kNone:
      case cssom::KeywordValue::kNormal:
      case cssom::KeywordValue::kRelative:
      case cssom::KeywordValue::kStatic:
      case cssom::KeywordValue::kVisible:
      default:
        NOTREACHED();
    }
  }

  void VisitLength(cssom::LengthValue* length) OVERRIDE {
    DCHECK_EQ(cssom::kPixelsUnit, length->unit());
    value_ = length->value();
  }

  void VisitPercentage(cssom::PercentageValue* percentage) OVERRIDE {
    value_ = percentage->value() * containing_block_size_;
    depends_on_containing_block_ = true;
  }

  const base::optional<float>& value() const { return value_; }
  bool depends_on_containing_block() const {
    return depends_on_containing_block_;
  }

 private:
  const float containing_block_size_;

  base::optional<float> value_;
  bool depends_on_containing_block_;

  DISALLOW_COPY_AND_ASSIGN(UsedSizeMetricProvider);
};

UsedBoxMetrics UsedBoxMetrics::ComputeMetrics(
    float containing_block_size, cssom::PropertyValue* position,
    cssom::PropertyValue* start_offset, cssom::PropertyValue* size,
    cssom::PropertyValue* end_offset) {
  UsedBoxMetrics ret;
  ret.offset_depends_on_containing_block = false;

  // Simply extract the metrics (and resolve percentages) from the style.  If
  // any of the metrics are set to auto, we leave them empty to express that.
  UsedSizeMetricProvider used_size(containing_block_size);
  size->Accept(&used_size);
  ret.size = used_size.value();
  ret.size_depends_on_containing_block =
      used_size.depends_on_containing_block();

  if (position == cssom::KeywordValue::GetAbsolute()) {
    // If we are a positioned element, we should also take the values of
    // start_offset and end_offset into account.
    //   http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
    // Note that not all cases listed in the specification above can be covered
    // here, since unless start_offset and end_offset are both specified,
    // determining the size requires a layout to occur.  Thus, in that case we
    // simply leave size undefined and the client can decide what to do.
    UsedSizeMetricProvider used_start_offset(containing_block_size);
    start_offset->Accept(&used_start_offset);
    ret.start_offset = used_start_offset.value();
    ret.offset_depends_on_containing_block =
        used_start_offset.depends_on_containing_block();

    UsedSizeMetricProvider used_end_offset(containing_block_size);
    end_offset->Accept(&used_end_offset);
    ret.end_offset = used_end_offset.value();
    ret.offset_depends_on_containing_block |=
        used_end_offset.depends_on_containing_block();
  }

  // After freshly retrieving the values, have a go at resolving the constraints
  // to see if we can fill in any blanks.
  ret.ResolveConstraints(containing_block_size);

  return ret;
}

void UsedBoxMetrics::ResolveConstraints(float containing_block_size) {
  // Check explicitly for 10.3.7 Case 5
  if (start_offset && end_offset && !size) {
    // Calculate the size from the specified values for start_offset and
    // end_offset.
    size = containing_block_size - *end_offset - *start_offset;
    size_depends_on_containing_block = offset_depends_on_containing_block;
  }

  // Determine the offset whether it is specified directly via 'start_offset' or
  // if it is specified indirectly via 'size' and 'end_offset'.
  if (!start_offset && end_offset && size) {
    // 10.3.7 Case 4.
    start_offset = containing_block_size - *end_offset - *size;
    offset_depends_on_containing_block |= size_depends_on_containing_block;
  }
}

}  // namespace layout
}  // namespace cobalt
