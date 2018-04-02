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

#include "cobalt/layout/used_style.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/transform_function_visitor.h"
#include "cobalt/cssom/transform_matrix_function_value.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/loader/mesh/mesh_cache.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rounded_corners.h"

namespace cobalt {
namespace layout {

namespace {

struct BackgroundImageTransformData {
  BackgroundImageTransformData(
      const math::SizeF& image_node_size,
      const math::Matrix3F& image_node_transform_matrix,
      const math::PointF& composition_node_translation)
      : image_node_size(image_node_size),
        image_node_transform_matrix(image_node_transform_matrix),
        composition_node_translation(composition_node_translation) {}

  math::SizeF image_node_size;

  // Transformation to be applied to the image's internal texture coordinates.
  math::Matrix3F image_node_transform_matrix;

  // Translation to be applied to the entire image.
  math::PointF composition_node_translation;
};

render_tree::FontStyle ConvertCSSOMFontValuesToRenderTreeFontStyle(
    cssom::FontStyleValue::Value style, cssom::FontWeightValue::Value weight) {
  render_tree::FontStyle::Weight font_weight =
      render_tree::FontStyle::kNormalWeight;
  switch (weight) {
    case cssom::FontWeightValue::kThinAka100:
      font_weight = render_tree::FontStyle::kThinWeight;
      break;
    case cssom::FontWeightValue::kExtraLightAka200:
      font_weight = render_tree::FontStyle::kExtraLightWeight;
      break;
    case cssom::FontWeightValue::kLightAka300:
      font_weight = render_tree::FontStyle::kLightWeight;
      break;
    case cssom::FontWeightValue::kNormalAka400:
      font_weight = render_tree::FontStyle::kNormalWeight;
      break;
    case cssom::FontWeightValue::kMediumAka500:
      font_weight = render_tree::FontStyle::kMediumWeight;
      break;
    case cssom::FontWeightValue::kSemiBoldAka600:
      font_weight = render_tree::FontStyle::kSemiBoldWeight;
      break;
    case cssom::FontWeightValue::kBoldAka700:
      font_weight = render_tree::FontStyle::kBoldWeight;
      break;
    case cssom::FontWeightValue::kExtraBoldAka800:
      font_weight = render_tree::FontStyle::kExtraBoldWeight;
      break;
    case cssom::FontWeightValue::kBlackAka900:
      font_weight = render_tree::FontStyle::kBlackWeight;
      break;
  }

  render_tree::FontStyle::Slant font_slant =
      style == cssom::FontStyleValue::kItalic
          ? render_tree::FontStyle::kItalicSlant
          : render_tree::FontStyle::kUprightSlant;

  return render_tree::FontStyle(font_weight, font_slant);
}

BackgroundImageTransformData GetImageTransformationData(
    UsedBackgroundSizeProvider* used_background_size_provider,
    UsedBackgroundPositionProvider* used_background_position_provider,
    UsedBackgroundRepeatProvider* used_background_repeat_provider,
    const math::RectF& frame, const math::SizeF& single_image_size) {
  // The initial value of following variables are for no-repeat horizontal and
  // vertical.
  math::SizeF image_node_size = single_image_size;
  float image_node_translate_matrix_x = 0.0f;
  float image_node_translate_matrix_y = 0.0f;
  float image_node_scale_matrix_x =
      used_background_size_provider->width() / single_image_size.width();
  float image_node_scale_matrix_y =
      used_background_size_provider->height() / single_image_size.height();
  float composition_node_translate_matrix_x =
      used_background_position_provider->translate_x();
  float composition_node_translate_matrix_y =
      used_background_position_provider->translate_y();

  if (used_background_repeat_provider->repeat_x() ||
      frame.width() < image_node_size.width()) {
    // When the background repeat horizontally or the width of frame is smaller
    // than the width of image, image node does the transform in horizontal
    // direction.
    image_node_size.set_width(frame.width());
    image_node_translate_matrix_x =
        used_background_position_provider->translate_x_relative_to_frame();
    image_node_scale_matrix_x =
        used_background_size_provider->width_scale_relative_to_frame();
    composition_node_translate_matrix_x = 0.0f;
  }

  if (used_background_repeat_provider->repeat_y() ||
      frame.height() < image_node_size.height()) {
    // When the background repeat vertically or the width of frame is smaller
    // than the height of image, image node does the transform in vertical
    // direction.
    image_node_size.set_height(frame.height());
    image_node_translate_matrix_y =
        used_background_position_provider->translate_y_relative_to_frame();
    image_node_scale_matrix_y =
        used_background_size_provider->height_scale_relative_to_frame();
    composition_node_translate_matrix_y = 0.0f;
  }

  BackgroundImageTransformData background_image_transform_data(
      image_node_size,
      math::TranslateMatrix(image_node_translate_matrix_x,
                            image_node_translate_matrix_y) *
          math::ScaleMatrix(image_node_scale_matrix_x,
                            image_node_scale_matrix_y),
      math::PointF(composition_node_translate_matrix_x + frame.x(),
                   composition_node_translate_matrix_y + frame.y()));
  return background_image_transform_data;
}

class UsedBackgroundTranslateProvider
    : public cssom::NotReachedPropertyValueVisitor {
 public:
  UsedBackgroundTranslateProvider(float frame_length, float image_length)
      : frame_length_(frame_length), image_length_(image_length) {}

  void VisitCalc(cssom::CalcValue* calc) override;

  // Returns the value based on the left top.
  float translate() { return translate_; }

 private:
  const float frame_length_;
  const float image_length_;

  float translate_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundTranslateProvider);
};

// A percentage for the horizontal offset is relative to (width of background
// positioning area - width of background image). A percentage for the vertical
// offset is relative to (height of background positioning area - height of
// background image), where the size of the image is the size given by
// 'background-size'.
//   https://www.w3.org/TR/css3-background/#the-background-position
void UsedBackgroundTranslateProvider::VisitCalc(cssom::CalcValue* calc) {
  DCHECK_EQ(cssom::kPixelsUnit, calc->length_value()->unit());

  translate_ =
      calc->percentage_value()->value() * (frame_length_ - image_length_) +
      calc->length_value()->value();
}

//   https://www.w3.org/TR/css3-background/#the-background-size
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

  void VisitKeyword(cssom::KeywordValue* keyword) override;
  void VisitLength(cssom::LengthValue* length) override;
  void VisitPercentage(cssom::PercentageValue* percentage) override;

  float scale() const { return scale_; }
  bool auto_keyword() const { return auto_keyword_; }

 private:
  const float frame_length_;
  const int image_length_;

  float scale_;
  bool auto_keyword_;

  DISALLOW_COPY_AND_ASSIGN(UsedBackgroundSizeScaleProvider);
};

void UsedBackgroundSizeScaleProvider::VisitKeyword(
    cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto: {
      auto_keyword_ = true;
      break;
    }
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAlternate:
    case cssom::KeywordValue::kAlternateReverse:
    case cssom::KeywordValue::kBackwards:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kBoth:
    case cssom::KeywordValue::kBottom:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kCurrentColor:
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kEnd:
    case cssom::KeywordValue::kEquirectangular:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kForwards:
    case cssom::KeywordValue::kFixed:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInfinite:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kLineThrough:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonoscopic:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kPreLine:
    case cssom::KeywordValue::kPreWrap:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kReverse:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
    case cssom::KeywordValue::kSolid:
    case cssom::KeywordValue::kStart:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kStereoscopicLeftRight:
    case cssom::KeywordValue::kStereoscopicTopBottom:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
      NOTREACHED();
  }
}

void UsedBackgroundSizeScaleProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  scale_ = length->value() / image_length_;
}

void UsedBackgroundSizeScaleProvider::VisitPercentage(
    cssom::PercentageValue* percentage) {
  scale_ = frame_length_ * percentage->value() / image_length_;
}

// TODO: Factor in generic families.
//   https://www.w3.org/TR/css3-fonts/#font-family-prop
class UsedFontFamilyProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedFontFamilyProvider(std::vector<std::string>* family_names)
      : family_names_(family_names) {}

  void VisitKeyword(cssom::KeywordValue* keyword) override;
  void VisitPropertyList(cssom::PropertyListValue* property_list) override;
  void VisitString(cssom::StringValue* percentage) override;

 private:
  std::vector<std::string>* family_names_;

  DISALLOW_COPY_AND_ASSIGN(UsedFontFamilyProvider);
};

void UsedFontFamilyProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kCurrentColor:
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
      family_names_->push_back(keyword->ToString());
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAlternate:
    case cssom::KeywordValue::kAlternateReverse:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBackwards:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kBoth:
    case cssom::KeywordValue::kBottom:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kEnd:
    case cssom::KeywordValue::kEquirectangular:
    case cssom::KeywordValue::kFixed:
    case cssom::KeywordValue::kForwards:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInfinite:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kLineThrough:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonoscopic:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kPreLine:
    case cssom::KeywordValue::kPreWrap:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kReverse:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSolid:
    case cssom::KeywordValue::kStart:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kStereoscopicLeftRight:
    case cssom::KeywordValue::kStereoscopicTopBottom:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
      NOTREACHED();
  }
}

void UsedFontFamilyProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list) {
  size_t size = property_list->value().size();
  family_names_->reserve(size);
  for (size_t i = 0; i < size; ++i) {
    property_list->value()[i]->Accept(this);
  }
}

void UsedFontFamilyProvider::VisitString(cssom::StringValue* string) {
  family_names_->push_back(string->value());
}

float GetFontSize(const scoped_refptr<cssom::PropertyValue>& font_size_refptr) {
  cssom::LengthValue* font_size_length =
      base::polymorphic_downcast<cssom::LengthValue*>(font_size_refptr.get());
  DCHECK_EQ(cssom::kPixelsUnit, font_size_length->unit());
  return font_size_length->value();
}

}  // namespace

UsedStyleProvider::UsedStyleProvider(
    dom::HTMLElementContext* html_element_context, dom::FontCache* font_cache,
    const AttachCameraNodeFunction& attach_camera_node_function,
    bool enable_image_animations)
    : font_cache_(font_cache),
      animated_image_tracker_(html_element_context->animated_image_tracker()),
      image_cache_(html_element_context->image_cache()),
      mesh_cache_(html_element_context->mesh_cache()),
      attach_camera_node_function_(attach_camera_node_function),
      enable_image_animations_(enable_image_animations) {}

scoped_refptr<dom::FontList> UsedStyleProvider::GetUsedFontList(
    const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_size_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_style_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_weight_refptr) {
  // Grab the font size prior to making the last font comparisons. The reason
  // that font size does not use the same property value comparisons as the
  // the rest of the properties is that the mechanism for generating a computed
  // font size results in numerous font size property values with the same
  // underlying size. Comparing the font size property pointer results in many
  // font lists with identical values incorrectly being treated as different.
  // This issue does not occur with the other font properties.
  float font_size = GetFontSize(font_size_refptr);

  // Check if the last font list matches the current font list. If it does, then
  // it can simply be returned.
  if (last_font_list_ != NULL && last_font_list_->size() == font_size &&
      last_font_family_refptr_.get() == font_family_refptr.get() &&
      last_font_style_refptr_.get() == font_style_refptr.get() &&
      last_font_weight_refptr_.get() == font_weight_refptr.get()) {
    return last_font_list_;
  }

  // Populate the font list key
  font_list_key_.family_names.clear();
  UsedFontFamilyProvider font_family_provider(&font_list_key_.family_names);
  font_family_refptr->Accept(&font_family_provider);

  cssom::FontStyleValue* font_style =
      base::polymorphic_downcast<cssom::FontStyleValue*>(
          font_style_refptr.get());
  cssom::FontWeightValue* font_weight =
      base::polymorphic_downcast<cssom::FontWeightValue*>(
          font_weight_refptr.get());
  font_list_key_.style = ConvertCSSOMFontValuesToRenderTreeFontStyle(
      font_style->value(), font_weight->value());

  font_list_key_.size = font_size;

  // Update the last font properties and grab the new last font list from the
  // font cache. In the case where it did not previously exist, the font cache
  // will create it.
  last_font_family_refptr_ = font_family_refptr;
  last_font_style_refptr_ = font_style_refptr;
  last_font_weight_refptr_ = font_weight_refptr;
  last_font_list_ = font_cache_->GetFontList(font_list_key_);

  return last_font_list_;
}

scoped_refptr<loader::image::Image> UsedStyleProvider::ResolveURLToImage(
    const GURL& url) {
  DCHECK(animated_image_tracker_);
  DCHECK(image_cache_);
  scoped_refptr<loader::image::Image> image =
      image_cache_->CreateCachedResource(url, loader::Origin())
          ->TryGetResource();
  if (image && image->IsAnimated()) {
    loader::image::AnimatedImage* animated_image =
        base::polymorphic_downcast<loader::image::AnimatedImage*>(image.get());
    animated_image_tracker_->RecordImage(url, animated_image);
  }
  return image;
}

scoped_refptr<loader::mesh::MeshProjection>
UsedStyleProvider::ResolveURLToMeshProjection(const GURL& url) {
  DCHECK(mesh_cache_);
  return mesh_cache_->CreateCachedResource(url, loader::Origin())
      ->TryGetResource();
}

void UsedStyleProvider::UpdateAnimatedImages() {
  animated_image_tracker_->ProcessRecordedImages();
}

void UsedStyleProvider::CleanupAfterLayout() {
  // Clear out the last font properties prior to requesting that the font cache
  // process inactive font lists. The reason for this is that the font cache
  // will look for any font lists where it holds the exclusive reference, and
  // the |last_font_list_| could potentially hold a second reference, thereby
  // interfering with the processing.
  last_font_family_refptr_ = NULL;
  last_font_style_refptr_ = NULL;
  last_font_weight_refptr_ = NULL;
  last_font_list_ = NULL;

  font_cache_->ProcessInactiveFontListsAndFonts();
}

UsedStyleProviderLayoutScope::UsedStyleProviderLayoutScope(
    UsedStyleProvider* used_style_provider)
    : used_style_provider_(used_style_provider) {}

UsedStyleProviderLayoutScope::~UsedStyleProviderLayoutScope() {
  used_style_provider_->CleanupAfterLayout();
}

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr) {
  cssom::RGBAColorValue* color =
      base::polymorphic_downcast<cssom::RGBAColorValue*>(color_refptr.get());
  return render_tree::ColorRGBA(color->value());
}

LayoutUnit GetUsedLength(
    const scoped_refptr<cssom::PropertyValue>& length_refptr) {
  cssom::LengthValue* length =
      base::polymorphic_downcast<cssom::LengthValue*>(length_refptr.get());
  DCHECK_EQ(length->unit(), cssom::kPixelsUnit);
  return LayoutUnit(length->value());
}

LayoutUnit GetUsedNonNegativeLength(
    const scoped_refptr<cssom::PropertyValue>& length_refptr) {
  cssom::LengthValue* length =
      base::polymorphic_downcast<cssom::LengthValue*>(length_refptr.get());
  DCHECK_EQ(length->unit(), cssom::kPixelsUnit);
  LayoutUnit layout_unit(length->value());
  if (layout_unit < LayoutUnit(0)) {
    DLOG(WARNING) << "Invalid non-negative layout length "
                  << layout_unit.toFloat() << ", original length was "
                  << length->value();
    layout_unit = LayoutUnit(0);
  }
  return layout_unit;
}

class UsedLengthValueProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  explicit UsedLengthValueProvider(LayoutUnit percentage_base,
                                   bool calc_permitted = false)
      : percentage_base_(percentage_base), calc_permitted_(calc_permitted) {}

  void VisitLength(cssom::LengthValue* length) override {
    depends_on_containing_block_ = false;

    DCHECK_EQ(cssom::kPixelsUnit, length->unit());
    used_length_ = LayoutUnit(length->value());
  }

  void VisitPercentage(cssom::PercentageValue* percentage) override {
    depends_on_containing_block_ = true;
    used_length_ = percentage->value() * percentage_base_;
  }

  void VisitCalc(cssom::CalcValue* calc) override {
    if (!calc_permitted_) {
      NOTREACHED();
    }
    depends_on_containing_block_ = true;
    used_length_ = LayoutUnit(calc->length_value()->value()) +
                   calc->percentage_value()->value() * percentage_base_;
  }

  bool depends_on_containing_block() const {
    return depends_on_containing_block_;
  }
  const base::optional<LayoutUnit>& used_length() const { return used_length_; }

 protected:
  bool depends_on_containing_block_;

 private:
  const LayoutUnit percentage_base_;
  const bool calc_permitted_;

  base::optional<LayoutUnit> used_length_;

  DISALLOW_COPY_AND_ASSIGN(UsedLengthValueProvider);
};

namespace {
float GetUsedLengthPercentageOrCalcValue(cssom::PropertyValue* property_value,
                                         float percentage_base) {
  UsedLengthValueProvider used_length_value_provider(
      LayoutUnit(percentage_base), true);
  property_value->Accept(&used_length_value_provider);
  return used_length_value_provider.used_length()->toFloat();
}
}  // namespace

UsedBackgroundNodeProvider::UsedBackgroundNodeProvider(
    const math::RectF& frame,
    const scoped_refptr<cssom::PropertyValue>& background_size,
    const scoped_refptr<cssom::PropertyValue>& background_position,
    const scoped_refptr<cssom::PropertyValue>& background_repeat,
    UsedStyleProvider* used_style_provider)
    : frame_(frame),
      background_size_(background_size),
      background_position_(background_position),
      background_repeat_(background_repeat),
      used_style_provider_(used_style_provider),
      is_opaque_(false) {}

void UsedBackgroundNodeProvider::VisitAbsoluteURL(
    cssom::AbsoluteURLValue* url_value) {
  // Deal with the case that background image is an image resource as opposed to
  // "linear-gradient".
  scoped_refptr<loader::image::Image> used_background_image =
      used_style_provider_->ResolveURLToImage(url_value->value());
  if (!used_background_image) {
    return;
  }

  UsedBackgroundSizeProvider used_background_size_provider(
      frame_.size(), used_background_image->GetSize());
  background_size_->Accept(&used_background_size_provider);

  math::SizeF single_image_size =
      math::SizeF(used_background_size_provider.width(),
                  used_background_size_provider.height());
  UsedBackgroundPositionProvider used_background_position_provider(
      frame_.size(), single_image_size);
  background_position_->Accept(&used_background_position_provider);

  UsedBackgroundRepeatProvider used_background_repeat_provider;
  background_repeat_->Accept(&used_background_repeat_provider);

  BackgroundImageTransformData image_transform_data =
      GetImageTransformationData(
          &used_background_size_provider, &used_background_position_provider,
          &used_background_repeat_provider, frame_, single_image_size);

  math::RectF image_rect(image_transform_data.composition_node_translation,
                         image_transform_data.image_node_size);

  is_opaque_ = used_background_image->IsOpaque() &&
               image_rect.x() <= frame_.x() && image_rect.y() <= frame_.y() &&
               image_rect.right() >= frame_.right() &&
               image_rect.bottom() >= frame_.bottom();

  if (!used_background_image->IsAnimated()) {
    loader::image::StaticImage* static_image =
        base::polymorphic_downcast<loader::image::StaticImage*>(
            used_background_image.get());
    DCHECK(static_image);
    background_node_ = new render_tree::ImageNode(
        static_image->image(), image_rect,
        image_transform_data.image_node_transform_matrix);
  } else {
    scoped_refptr<loader::image::AnimatedImage> animated_image =
        base::polymorphic_downcast<loader::image::AnimatedImage*>(
            used_background_image.get());
    scoped_refptr<render_tree::ImageNode> image_node =
        new render_tree::ImageNode(
            animated_image->GetFrameProvider()->GetFrame(), image_rect,
            image_transform_data.image_node_transform_matrix);
    if (!used_style_provider_->enable_image_animations()) {
      background_node_ = image_node;
    } else {
      render_tree::animations::AnimateNode::Builder animate_node_builder;
      animate_node_builder.Add(
          image_node,
          base::Bind(&loader::image::AnimatedImage::AnimateCallback,
                     animated_image->GetFrameProvider(), image_rect,
                     image_transform_data.image_node_transform_matrix));

      background_node_ = new render_tree::animations::AnimateNode(
          animate_node_builder, image_node);
    }
  }
}

namespace {
std::pair<math::PointF, math::PointF> LinearGradientPointsFromDirection(
    cssom::LinearGradientValue::SideOrCorner from,
    const math::SizeF& frame_size) {
  switch (from) {
    case cssom::LinearGradientValue::kBottom:
      return std::make_pair(math::PointF(0, 0),
                            math::PointF(0, frame_size.height()));
    case cssom::LinearGradientValue::kBottomLeft:
      return std::make_pair(math::PointF(frame_size.width(), 0),
                            math::PointF(0, frame_size.height()));
    case cssom::LinearGradientValue::kBottomRight:
      return std::make_pair(
          math::PointF(0, 0),
          math::PointF(frame_size.width(), frame_size.height()));
    case cssom::LinearGradientValue::kLeft:
      return std::make_pair(math::PointF(frame_size.width(), 0),
                            math::PointF(0, 0));
    case cssom::LinearGradientValue::kRight:
      return std::make_pair(math::PointF(0, 0),
                            math::PointF(frame_size.width(), 0));
    case cssom::LinearGradientValue::kTop:
      return std::make_pair(math::PointF(0, frame_size.height()),
                            math::PointF(0, 0));
    case cssom::LinearGradientValue::kTopLeft:
      return std::make_pair(
          math::PointF(frame_size.width(), frame_size.height()),
          math::PointF(0, 0));
    case cssom::LinearGradientValue::kTopRight:
      return std::make_pair(math::PointF(0, frame_size.height()),
                            math::PointF(frame_size.width(), 0));
  }
  NOTREACHED();
  return std::make_pair(math::PointF(0, 0), math::PointF(0, 0));
}

std::pair<math::PointF, math::PointF> LinearGradientPointsFromAngle(
    float angle_in_radians, const math::SizeF& frame_size) {
  // The method of defining the source and destination points for the linear
  // gradient are defined here:
  //   https://www.w3.org/TR/2012/CR-css3-images-20120417/#linear-gradients

  // The angle specified by linear gradient has "up" as its origin direction
  // and rotates clockwise as the angle increases.  We must convert this to
  // an angle that has "right" as its origin and moves counter clockwise before
  // we can pass it into the trigonometric functions cos() and sin().
  float ccw_angle_from_right = -angle_in_radians + static_cast<float>(M_PI / 2);

  return render_tree::LinearGradientPointsFromAngle(ccw_angle_from_right,
                                                    frame_size);
}

// The specifications indicate that if positions are not specified for color
// stops, then they should be filled in automatically by evenly spacing them
// between the two neighbooring color stops that DO have positions specified.
// This function implements this.  It assumes that unspecified position values
// are indicated by a value of -1.0f.
void InterpolateUnspecifiedColorStopPositions(
    render_tree::ColorStopList* color_stops) {
  size_t last_specified_index = 0;
  for (size_t i = 1; i < color_stops->size(); ++i) {
    const render_tree::ColorStop& color_stop = (*color_stops)[i];
    if (color_stop.position >= 0.0f) {
      // This is a specified value, so we may need to fill in previous
      // unspecified values.
      if (last_specified_index != i - 1) {
        float step_size = (color_stop.position -
                           (*color_stops)[last_specified_index].position) /
                          (i - last_specified_index);

        for (size_t j = last_specified_index + 1; j < i; ++j) {
          DCHECK_LT((*color_stops)[j].position, 0);
          (*color_stops)[j].position = (j - last_specified_index) * step_size;
        }
      }
      last_specified_index = i;
    }
  }
}

// Compares ColorStops by position.
bool ColorStopPositionComparator(const render_tree::ColorStop& x,
                                 const render_tree::ColorStop& y) {
  return x.position < y.position;
}

render_tree::ColorStopList ConvertToRenderTreeColorStopList(
    const cssom::ColorStopList& css_color_stop_list,
    float gradient_line_length) {
  render_tree::ColorStopList ret;

  ret.reserve(css_color_stop_list.size());

  // The description of this process is defined here:
  //   https://www.w3.org/TR/css3-images/#color-stop-syntax
  float largest_position = 0.0f;
  const float kMaxPositionSupported = 1.0f;
  for (size_t i = 0; i < css_color_stop_list.size(); ++i) {
    const cssom::ColorStop& css_color_stop = *css_color_stop_list[i];

    render_tree::ColorRGBA render_tree_color =
        GetUsedColor(css_color_stop.rgba());

    const scoped_refptr<cssom::PropertyValue>& css_position =
        css_color_stop.position();

    float render_tree_position;
    if (css_position) {
      // If the position is specified, enter it directly.
      if (css_position->GetTypeId() == base::GetTypeId<cssom::LengthValue>()) {
        float length_value =
            base::polymorphic_downcast<cssom::LengthValue*>(css_position.get())
                ->value();
        render_tree_position = length_value / gradient_line_length;
      } else {
        render_tree_position =
            base::polymorphic_downcast<cssom::PercentageValue*>(
                css_position.get())
                ->value();
      }

      // Ensure that it is larger than all previous stop positions.
      render_tree_position = std::max(largest_position, render_tree_position);
      DLOG_IF(WARNING, render_tree_position > kMaxPositionSupported)
          << "Color stop's position which is larger than 1.0 is not supported";
      render_tree_position =
          std::min(render_tree_position, kMaxPositionSupported);
      largest_position = render_tree_position;
    } else {
      // If the position is not specified, fill it in as 0 if it is the first,
      // or 1 if it is last.
      if (i == 0) {
        render_tree_position = 0.0f;
      } else if (i == css_color_stop_list.size() - 1) {
        render_tree_position = 1.0f;
      } else {
        // Otherwise, we set it to -1.0f and we'll come back to it later to
        // interpolate evenly between the two closest specified values.
        render_tree_position = -1.0f;
      }
    }

    ret.push_back(
        render_tree::ColorStop(render_tree_position, render_tree_color));
  }

  InterpolateUnspecifiedColorStopPositions(&ret);

  // According to the spec @ https://www.w3.org/TR/css3-images/#linear-gradients
  // the color-stops can be in unsorted order.  The color-stops are sorted
  // to make the rendering code easier to write and faster to execute.
  std::sort(ret.begin(), ret.end(), ColorStopPositionComparator);

  return ret;
}
}  // namespace

void UsedBackgroundNodeProvider::VisitLinearGradient(
    cssom::LinearGradientValue* linear_gradient_value) {
  std::pair<math::PointF, math::PointF> source_and_dest;
  if (linear_gradient_value->side_or_corner()) {
    source_and_dest = LinearGradientPointsFromDirection(
        *linear_gradient_value->side_or_corner(), frame_.size());
  } else {
    source_and_dest = LinearGradientPointsFromAngle(
        *linear_gradient_value->angle_in_radians(), frame_.size());
  }

  render_tree::ColorStopList color_stop_list = ConvertToRenderTreeColorStopList(
      linear_gradient_value->color_stop_list(),
      (source_and_dest.second - source_and_dest.first).Length());

  scoped_ptr<render_tree::LinearGradientBrush> brush(
      new render_tree::LinearGradientBrush(
          source_and_dest.first, source_and_dest.second, color_stop_list));

  background_node_ =
      new render_tree::RectNode(frame_, brush.PassAs<render_tree::Brush>());
}

namespace {

std::pair<float, float> RadialGradientAxesFromSizeKeyword(
    cssom::RadialGradientValue::Shape shape,
    cssom::RadialGradientValue::SizeKeyword size, const math::PointF& center,
    const math::SizeF& frame_size) {
  float closest_side_x =
      std::min(std::abs(center.x()), std::abs(frame_size.width() - center.x()));
  float closest_side_y = std::min(std::abs(center.y()),
                                  std::abs(frame_size.height() - center.y()));
  float farthest_side_x =
      std::max(std::abs(center.x()), std::abs(frame_size.width() - center.x()));
  float farthest_side_y = std::max(std::abs(center.y()),
                                   std::abs(frame_size.height() - center.y()));

  math::Vector2dF to_top_left(center.x(), center.y());
  math::Vector2dF to_top_right(frame_size.width() - center.x(), center.y());
  math::Vector2dF to_bottom_right(frame_size.width() - center.x(),
                                  frame_size.height() - center.y());
  math::Vector2dF to_bottom_left(center.x(), frame_size.height() - center.y());
  math::Vector2dF* corners[] = {&to_top_left, &to_top_right, &to_bottom_right,
                                &to_bottom_left};

  math::Vector2dF* closest_corner = corners[0];
  double closest_distance_sq = closest_corner->LengthSquared();
  for (size_t i = 1; i < arraysize(corners); ++i) {
    double length_sq = corners[i]->LengthSquared();
    if (length_sq < closest_distance_sq) {
      closest_distance_sq = length_sq;
      closest_corner = corners[i];
    }
  }

  math::Vector2dF* farthest_corner = corners[0];
  double farthest_distance_sq = farthest_corner->LengthSquared();
  for (size_t i = 1; i < arraysize(corners); ++i) {
    double length_sq = corners[i]->LengthSquared();
    if (length_sq > farthest_distance_sq) {
      farthest_distance_sq = length_sq;
      farthest_corner = corners[i];
    }
  }

  switch (shape) {
    case cssom::RadialGradientValue::kCircle: {
      switch (size) {
        case cssom::RadialGradientValue::kClosestSide: {
          float closest_side = std::min(closest_side_x, closest_side_y);
          return std::make_pair(closest_side, closest_side);
        }
        case cssom::RadialGradientValue::kFarthestSide: {
          float farthest_side = std::max(farthest_side_x, farthest_side_y);
          return std::make_pair(farthest_side, farthest_side);
        }
        case cssom::RadialGradientValue::kClosestCorner: {
          float distance = closest_corner->Length();
          return std::make_pair(distance, distance);
        }
        case cssom::RadialGradientValue::kFarthestCorner: {
          float distance = farthest_corner->Length();
          return std::make_pair(distance, distance);
        }
      }
    } break;
    case cssom::RadialGradientValue::kEllipse: {
      switch (size) {
        case cssom::RadialGradientValue::kClosestSide: {
          return std::make_pair(closest_side_x, closest_side_y);
        }
        case cssom::RadialGradientValue::kFarthestSide: {
          return std::make_pair(farthest_side_x, farthest_side_y);
        }
        // For the next two cases, we must compute the ellipse that touches the
        // closest [or farthest] corner, but has the same ratio as if we had
        // selected the closest [or farthest] side.
        case cssom::RadialGradientValue::kClosestCorner: {
          float ratio = closest_side_y / closest_side_x;
          float y_over_ratio = closest_corner->y() / ratio;
          float horizontal_axis = static_cast<float>(
              sqrt(closest_corner->x() * closest_corner->x() +
                   y_over_ratio * y_over_ratio));
          return std::make_pair(horizontal_axis, horizontal_axis * ratio);
        }
        case cssom::RadialGradientValue::kFarthestCorner: {
          float ratio = farthest_side_y / farthest_side_x;
          float y_over_ratio = farthest_corner->y() / ratio;
          float horizontal_axis = static_cast<float>(
              sqrt(farthest_corner->x() * farthest_corner->x() +
                   y_over_ratio * y_over_ratio));
          return std::make_pair(horizontal_axis, horizontal_axis * ratio);
        }
      }
    } break;
  }

  NOTREACHED();
  return std::make_pair(0.0f, 0.0f);
}

std::pair<float, float> RadialGradientAxesFromSizeValue(
    cssom::RadialGradientValue::Shape shape,
    const cssom::PropertyListValue& size, const math::SizeF& frame_size) {
  switch (shape) {
    case cssom::RadialGradientValue::kCircle: {
      DCHECK_EQ(1U, size.value().size());
      cssom::LengthValue* size_as_length =
          base::polymorphic_downcast<cssom::LengthValue*>(
              size.value()[0].get());
      return std::make_pair(size_as_length->value(), size_as_length->value());
    } break;
    case cssom::RadialGradientValue::kEllipse: {
      DCHECK_EQ(2U, size.value().size());
      float radii[2];
      float dimensions[2] = {frame_size.width(), frame_size.height()};
      for (size_t i = 0; i < 2; ++i) {
        radii[i] = GetUsedLengthPercentageOrCalcValue(size.value()[i].get(),
                                                      dimensions[i]);
      }
      return std::make_pair(radii[0], radii[1]);
    } break;
  }

  NOTREACHED();
  return std::make_pair(0.0f, 0.0f);
}

math::PointF RadialGradientCenterFromCSSOM(
    const scoped_refptr<cssom::PropertyListValue>& position,
    const math::SizeF& frame_size) {
  if (!position) {
    return math::PointF(frame_size.width() / 2.0f, frame_size.height() / 2.0f);
  }

  DCHECK_EQ(position->value().size(), 2);
  return math::PointF(GetUsedLengthPercentageOrCalcValue(
                          position->value()[0].get(), frame_size.width()),
                      GetUsedLengthPercentageOrCalcValue(
                          position->value()[1].get(), frame_size.height()));
}

}  // namespace

void UsedBackgroundNodeProvider::VisitRadialGradient(
    cssom::RadialGradientValue* radial_gradient_value) {
  math::PointF center = RadialGradientCenterFromCSSOM(
      radial_gradient_value->position(), frame_.size());

  std::pair<float, float> major_and_minor_axes;
  if (radial_gradient_value->size_keyword()) {
    major_and_minor_axes = RadialGradientAxesFromSizeKeyword(
        radial_gradient_value->shape(), *radial_gradient_value->size_keyword(),
        center, frame_.size());
  } else {
    major_and_minor_axes = RadialGradientAxesFromSizeValue(
        radial_gradient_value->shape(), *radial_gradient_value->size_value(),
        frame_.size());
  }

  render_tree::ColorStopList color_stop_list = ConvertToRenderTreeColorStopList(
      radial_gradient_value->color_stop_list(), major_and_minor_axes.first);

  scoped_ptr<render_tree::RadialGradientBrush> brush(
      new render_tree::RadialGradientBrush(center, major_and_minor_axes.first,
                                           major_and_minor_axes.second,
                                           color_stop_list));

  background_node_ =
      new render_tree::RectNode(frame_, brush.PassAs<render_tree::Brush>());
}

//   https://www.w3.org/TR/css3-background/#the-background-position
UsedBackgroundPositionProvider::UsedBackgroundPositionProvider(
    const math::SizeF& frame_size, const math::SizeF& image_actual_size)
    : frame_size_(frame_size), image_actual_size_(image_actual_size) {}

void UsedBackgroundPositionProvider::VisitPropertyList(
    cssom::PropertyListValue* property_list_value) {
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

UsedBackgroundRepeatProvider::UsedBackgroundRepeatProvider()
    : repeat_x_(false), repeat_y_(false) {}

void UsedBackgroundRepeatProvider::VisitPropertyList(
    cssom::PropertyListValue* background_repeat_list) {
  DCHECK_EQ(background_repeat_list->value().size(), 2);

  repeat_x_ =
      background_repeat_list->value()[0] == cssom::KeywordValue::GetRepeat()
          ? true
          : false;

  repeat_y_ =
      background_repeat_list->value()[1] == cssom::KeywordValue::GetRepeat()
          ? true
          : false;
}

UsedBackgroundSizeProvider::UsedBackgroundSizeProvider(
    const math::SizeF& frame_size, const math::Size& image_size)
    : frame_size_(frame_size),
      image_size_(image_size),
      width_(1.0f),
      height_(1.0f) {}

// The first value gives the width of the corresponding image, and the second
// value gives its height.
//   https://www.w3.org/TR/css3-background/#the-background-size
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
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAlternate:
    case cssom::KeywordValue::kAlternateReverse:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBackwards:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBoth:
    case cssom::KeywordValue::kBottom:
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kCurrentColor:
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kEnd:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kFixed:
    case cssom::KeywordValue::kForwards:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInfinite:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kLineThrough:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonoscopic:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kPreLine:
    case cssom::KeywordValue::kPreWrap:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kReverse:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
    case cssom::KeywordValue::kSolid:
    case cssom::KeywordValue::kStart:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kStereoscopicLeftRight:
    case cssom::KeywordValue::kStereoscopicTopBottom:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void UsedBackgroundSizeProvider::ConvertWidthAndHeightScale(
    float width_scale, float height_scale) {
  if (frame_size_.width() < 0 || frame_size_.height() < 0) {
    DLOG(WARNING) << "Frame size is negative.";
    width_ = height_ = 0.0f;
    return;
  }

  width_ = width_scale * image_size_.width();
  height_ = height_scale * image_size_.height();
}

UsedBorderRadiusProvider::UsedBorderRadiusProvider(
    const math::SizeF& frame_size)
    : frame_size_(frame_size) {}

void UsedBorderRadiusProvider::VisitLength(cssom::LengthValue* length) {
  if (length->value() > 0) {
    rounded_corner_.emplace(length->value(), length->value());
  } else {
    rounded_corner_ = base::nullopt;
  }
}

void UsedBorderRadiusProvider::VisitPercentage(
    cssom::PercentageValue* percentage) {
  if (percentage->value() > 0) {
    rounded_corner_.emplace(percentage->value() * frame_size_.width(),
                            percentage->value() * frame_size_.height());
  } else {
    rounded_corner_ = base::nullopt;
  }
}

UsedLineHeightProvider::UsedLineHeightProvider(
    const render_tree::FontMetrics& font_metrics,
    const scoped_refptr<cssom::PropertyValue>& font_size)
    : font_metrics_(font_metrics), font_size_(font_size) {}

void UsedLineHeightProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  DCHECK_EQ(cssom::KeywordValue::kNormal, keyword->value());
  used_line_height_ = LayoutUnit(font_metrics_.em_box_height());
  UpdateHalfLeading();
}

void UsedLineHeightProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit());
  used_line_height_ = LayoutUnit(length->value());
  UpdateHalfLeading();
}

void UsedLineHeightProvider::VisitNumber(cssom::NumberValue* length) {
  float font_size = GetFontSize(font_size_);
  // The used value of the property is this number multiplied by the element's
  // font size.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  used_line_height_ = LayoutUnit(length->value() * font_size);
  UpdateHalfLeading();
}

void UsedLineHeightProvider::UpdateHalfLeading() {
  // Determine the leading L, where L = "line-height" - AD,
  // AD = A (ascent) + D (descent).
  //   https://www.w3.org/TR/CSS21/visudet.html#leading
  half_leading_ = (used_line_height_ - LayoutUnit(font_metrics_.ascent() +
                                                  font_metrics_.descent())) /
                  2;
}

// A percentage for the horizontal offset is relative to the width of the
// bounding box. A percentage for the vertical offset is relative to height of
// the bounding box. A length value gives a fixed length as the offset.
// The value for the horizontal and vertical offset represent an offset from the
// top left corner of the bounding box.
//  https://www.w3.org/TR/css3-transforms/#transform-origin-property
math::Vector2dF GetTransformOrigin(const math::RectF& used_rect,
                                   cssom::PropertyValue* value) {
  const cssom::PropertyListValue* property_list =
      base::polymorphic_downcast<const cssom::PropertyListValue*>(value);

  DCHECK_EQ(property_list->value().size(), 3u);
  const cssom::CalcValue* horizontal =
      base::polymorphic_downcast<const cssom::CalcValue*>(
          property_list->value()[0].get());
  float x_within_border_box =
      horizontal->percentage_value()->value() * used_rect.width() +
      horizontal->length_value()->value();

  const cssom::CalcValue* vertical =
      base::polymorphic_downcast<const cssom::CalcValue*>(
          property_list->value()[1].get());
  float y_within_border_box =
      vertical->percentage_value()->value() * used_rect.height() +
      vertical->length_value()->value();

  return math::Vector2dF(used_rect.x() + x_within_border_box,
                         used_rect.y() + y_within_border_box);
}

cssom::TransformMatrix GetTransformMatrix(cssom::PropertyValue* value) {
  if (value->GetTypeId() ==
      base::GetTypeId<cssom::TransformMatrixFunctionValue>()) {
    cssom::TransformMatrixFunctionValue* matrix_function =
        base::polymorphic_downcast<cssom::TransformMatrixFunctionValue*>(value);
    return matrix_function->value();
  } else {
    cssom::TransformFunctionListValue* transform_list =
        base::polymorphic_downcast<cssom::TransformFunctionListValue*>(value);
    return transform_list->ToMatrix();
  }
}

namespace {

class UsedLengthProvider : public UsedLengthValueProvider {
 public:
  explicit UsedLengthProvider(LayoutUnit percentage_base)
      : UsedLengthValueProvider(percentage_base) {}

  void VisitKeyword(cssom::KeywordValue* keyword) override {
    switch (keyword->value()) {
      case cssom::KeywordValue::kAuto:
        depends_on_containing_block_ = true;

        // Leave |used_length_| in disengaged state to indicate that "auto"
        // was the value.
        break;

      case cssom::KeywordValue::kAbsolute:
      case cssom::KeywordValue::kAlternate:
      case cssom::KeywordValue::kAlternateReverse:
      case cssom::KeywordValue::kBackwards:
      case cssom::KeywordValue::kBaseline:
      case cssom::KeywordValue::kBlock:
      case cssom::KeywordValue::kBoth:
      case cssom::KeywordValue::kBottom:
      case cssom::KeywordValue::kBreakWord:
      case cssom::KeywordValue::kCenter:
      case cssom::KeywordValue::kClip:
      case cssom::KeywordValue::kContain:
      case cssom::KeywordValue::kCover:
      case cssom::KeywordValue::kCurrentColor:
      case cssom::KeywordValue::kCursive:
      case cssom::KeywordValue::kEllipsis:
      case cssom::KeywordValue::kEnd:
      case cssom::KeywordValue::kFantasy:
      case cssom::KeywordValue::kForwards:
      case cssom::KeywordValue::kFixed:
      case cssom::KeywordValue::kHidden:
      case cssom::KeywordValue::kInfinite:
      case cssom::KeywordValue::kInherit:
      case cssom::KeywordValue::kInitial:
      case cssom::KeywordValue::kInline:
      case cssom::KeywordValue::kInlineBlock:
      case cssom::KeywordValue::kLeft:
      case cssom::KeywordValue::kLineThrough:
      case cssom::KeywordValue::kMiddle:
      case cssom::KeywordValue::kMonoscopic:
      case cssom::KeywordValue::kMonospace:
      case cssom::KeywordValue::kNone:
      case cssom::KeywordValue::kNoRepeat:
      case cssom::KeywordValue::kNormal:
      case cssom::KeywordValue::kNoWrap:
      case cssom::KeywordValue::kPre:
      case cssom::KeywordValue::kPreLine:
      case cssom::KeywordValue::kPreWrap:
      case cssom::KeywordValue::kRelative:
      case cssom::KeywordValue::kRepeat:
      case cssom::KeywordValue::kReverse:
      case cssom::KeywordValue::kRight:
      case cssom::KeywordValue::kSansSerif:
      case cssom::KeywordValue::kSerif:
      case cssom::KeywordValue::kSolid:
      case cssom::KeywordValue::kStart:
      case cssom::KeywordValue::kStatic:
      case cssom::KeywordValue::kStereoscopicLeftRight:
      case cssom::KeywordValue::kStereoscopicTopBottom:
      case cssom::KeywordValue::kTop:
      case cssom::KeywordValue::kUppercase:
      case cssom::KeywordValue::kVisible:
      default:
        NOTREACHED();
    }
  }
};

class UsedMaxLengthProvider : public UsedLengthValueProvider {
 public:
  explicit UsedMaxLengthProvider(LayoutUnit percentage_base)
      : UsedLengthValueProvider(percentage_base) {}

  void VisitKeyword(cssom::KeywordValue* keyword) override {
    switch (keyword->value()) {
      case cssom::KeywordValue::kNone:
        depends_on_containing_block_ = true;

        // Leave |used_length_| in disengaged state to indicate that "none"
        // was the value.
        break;

      case cssom::KeywordValue::kAbsolute:
      case cssom::KeywordValue::kAlternate:
      case cssom::KeywordValue::kAlternateReverse:
      case cssom::KeywordValue::kAuto:
      case cssom::KeywordValue::kBackwards:
      case cssom::KeywordValue::kBaseline:
      case cssom::KeywordValue::kBlock:
      case cssom::KeywordValue::kBoth:
      case cssom::KeywordValue::kBottom:
      case cssom::KeywordValue::kBreakWord:
      case cssom::KeywordValue::kCenter:
      case cssom::KeywordValue::kClip:
      case cssom::KeywordValue::kContain:
      case cssom::KeywordValue::kCover:
      case cssom::KeywordValue::kCurrentColor:
      case cssom::KeywordValue::kCursive:
      case cssom::KeywordValue::kEllipsis:
      case cssom::KeywordValue::kEnd:
      case cssom::KeywordValue::kFantasy:
      case cssom::KeywordValue::kForwards:
      case cssom::KeywordValue::kFixed:
      case cssom::KeywordValue::kHidden:
      case cssom::KeywordValue::kInfinite:
      case cssom::KeywordValue::kInherit:
      case cssom::KeywordValue::kInitial:
      case cssom::KeywordValue::kInline:
      case cssom::KeywordValue::kInlineBlock:
      case cssom::KeywordValue::kLeft:
      case cssom::KeywordValue::kLineThrough:
      case cssom::KeywordValue::kMiddle:
      case cssom::KeywordValue::kMonoscopic:
      case cssom::KeywordValue::kMonospace:
      case cssom::KeywordValue::kNoRepeat:
      case cssom::KeywordValue::kNormal:
      case cssom::KeywordValue::kNoWrap:
      case cssom::KeywordValue::kPre:
      case cssom::KeywordValue::kPreLine:
      case cssom::KeywordValue::kPreWrap:
      case cssom::KeywordValue::kRelative:
      case cssom::KeywordValue::kRepeat:
      case cssom::KeywordValue::kReverse:
      case cssom::KeywordValue::kRight:
      case cssom::KeywordValue::kSansSerif:
      case cssom::KeywordValue::kSerif:
      case cssom::KeywordValue::kSolid:
      case cssom::KeywordValue::kStart:
      case cssom::KeywordValue::kStatic:
      case cssom::KeywordValue::kStereoscopicLeftRight:
      case cssom::KeywordValue::kStereoscopicTopBottom:
      case cssom::KeywordValue::kTop:
      case cssom::KeywordValue::kUppercase:
      case cssom::KeywordValue::kVisible:
      default:
        NOTREACHED();
    }
  }
};

}  // namespace

base::optional<LayoutUnit> GetUsedLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#position-props
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->left()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedTopIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to height of containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#position-props
  UsedLengthProvider used_length_provider(containing_block_size.height());
  computed_style->top()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedRightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#position-props
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->right()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to height of containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#position-props
  UsedLengthProvider used_length_provider(containing_block_size.height());
  computed_style->bottom()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedWidthIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#the-width-property
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->width()->Accept(&used_length_provider);
  if (width_depends_on_containing_block != NULL) {
    *width_depends_on_containing_block =
        used_length_provider.depends_on_containing_block();
  }
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMaxHeightIfNotNone(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* height_depends_on_containing_block) {
  // Percentages: refer to height of containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#propdef-max-height
  UsedMaxLengthProvider used_length_provider(containing_block_size.height());
  computed_style->max_height()->Accept(&used_length_provider);
  if (height_depends_on_containing_block != NULL) {
    *height_depends_on_containing_block =
        used_length_provider.depends_on_containing_block();
  }
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMaxWidthIfNotNone(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#propdef-max-width
  UsedMaxLengthProvider used_length_provider(containing_block_size.width());
  computed_style->max_width()->Accept(&used_length_provider);
  if (width_depends_on_containing_block != NULL) {
    *width_depends_on_containing_block =
        used_length_provider.depends_on_containing_block();
  }
  return used_length_provider.used_length();
}

LayoutUnit GetUsedMinHeight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* height_depends_on_containing_block) {
  // Percentages: refer to height of containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#propdef-max-height
  UsedLengthValueProvider used_length_provider(containing_block_size.height());
  computed_style->min_height()->Accept(&used_length_provider);
  if (height_depends_on_containing_block != NULL) {
    *height_depends_on_containing_block =
        used_length_provider.depends_on_containing_block();
  }
  return *used_length_provider.used_length();
}

LayoutUnit GetUsedMinWidth(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size,
    bool* width_depends_on_containing_block) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#propdef-min-width
  UsedLengthValueProvider used_length_provider(containing_block_size.width());
  computed_style->min_width()->Accept(&used_length_provider);
  if (width_depends_on_containing_block != NULL) {
    *width_depends_on_containing_block =
        used_length_provider.depends_on_containing_block();
  }
  return *used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedHeightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // The percentage is calculated with respect to the height of the generated
  // box's containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
  UsedLengthProvider used_length_provider(containing_block_size.height());
  computed_style->height()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMarginLeftIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#margin-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->margin_left()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMarginTopIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#margin-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->margin_top()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMarginRightIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#margin-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->margin_right()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

base::optional<LayoutUnit> GetUsedMarginBottomIfNotAuto(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#margin-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->margin_bottom()->Accept(&used_length_provider);
  return used_length_provider.used_length();
}

LayoutUnit GetUsedBorderLeft(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style) {
  return LayoutUnit(base::polymorphic_downcast<const cssom::LengthValue*>(
                        computed_style->border_left_width().get())
                        ->value());
}

LayoutUnit GetUsedBorderTop(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style) {
  return LayoutUnit(base::polymorphic_downcast<const cssom::LengthValue*>(
                        computed_style->border_top_width().get())
                        ->value());
}

LayoutUnit GetUsedBorderRight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style) {
  return LayoutUnit(base::polymorphic_downcast<const cssom::LengthValue*>(
                        computed_style->border_right_width().get())
                        ->value());
}

LayoutUnit GetUsedBorderBottom(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style) {
  return LayoutUnit(base::polymorphic_downcast<const cssom::LengthValue*>(
                        computed_style->border_bottom_width().get())
                        ->value());
}

LayoutUnit GetUsedPaddingLeft(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#padding-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->padding_left()->Accept(&used_length_provider);
  return *used_length_provider.used_length();
}

LayoutUnit GetUsedPaddingTop(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#padding-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->padding_top()->Accept(&used_length_provider);
  return *used_length_provider.used_length();
}

LayoutUnit GetUsedPaddingRight(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#padding-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->padding_right()->Accept(&used_length_provider);
  return *used_length_provider.used_length();
}

LayoutUnit GetUsedPaddingBottom(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style,
    const SizeLayoutUnit& containing_block_size) {
  // Percentages: refer to width of containing block.
  //   https://www.w3.org/TR/CSS21/box.html#padding-properties
  UsedLengthProvider used_length_provider(containing_block_size.width());
  computed_style->padding_bottom()->Accept(&used_length_provider);
  return *used_length_provider.used_length();
}

}  // namespace layout
}  // namespace cobalt
