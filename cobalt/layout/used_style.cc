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
  float frame_length_;
  int image_length_;
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

//   http://www.w3.org/TR/css3-background/#the-background-size
void UsedBackgroundSizeProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list_value) {
  DCHECK_GT(property_list_value->value().size(), 0);
  DCHECK_LE(property_list_value->value().size(), 2);

  UsedBackgroundSizeScaleProvider used_background_width_provider(
      frame_size_.width(), image_size_.width());
  property_list_value->value()[0]->Accept(&used_background_width_provider);

  UsedBackgroundSizeScaleProvider used_background_height_provider(
      frame_size_.height(), image_size_.height());
  if (property_list_value->value().size() == 2) {
    property_list_value->value()[1]->Accept(&used_background_height_provider);
  } else {
    // If only one value is given, the second is assumed to be 'auto'.
    cssom::KeywordValue::GetAuto()->Accept(&used_background_height_provider);
  }

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

void UsedHeightProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto:
      VisitAuto();
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

void UsedHeightProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  used_height_ = length->value();
}

// The percentage is calculated with respect to the height of the containing
// block.
//   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
void UsedHeightProvider::VisitPercentage(cssom::PercentageValue* percentage) {
  used_height_ = containing_block_height_ * percentage->value();
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

void UsedWidthProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto:
      VisitAuto();
      width_depends_on_containing_block_ = true;
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

void UsedWidthProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  used_width_ = length->value();
  width_depends_on_containing_block_ = false;
}

// Percentages: refer to width of containing block.
//   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
void UsedWidthProvider::VisitPercentage(cssom::PercentageValue* percentage) {
  used_width_ = containing_block_width_ * percentage->value();
  width_depends_on_containing_block_ = true;
}

}  // namespace layout
}  // namespace cobalt
