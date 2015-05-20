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
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"

namespace cobalt {
namespace layout {

namespace {

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

}  // namespace

UsedStyleProvider::UsedStyleProvider(
    render_tree::ResourceProvider* resource_provider,
    loader::ImageCache* image_cache)
    : resource_provider_(resource_provider), image_cache_(image_cache) {}

scoped_refptr<render_tree::Font> UsedStyleProvider::GetUsedFont(
    const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_size_refptr) const {
  cssom::StringValue* font_family =
      base::polymorphic_downcast<cssom::StringValue*>(font_family_refptr.get());
  cssom::LengthValue* font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(font_size_refptr.get());
  DCHECK_EQ(cssom::kPixelsUnit, font_size->unit());

  // TODO(***REMOVED***): Implement font style.
  return resource_provider_->GetPreInstalledFont(
      font_family->value().c_str(), render_tree::kNormal, font_size->value());
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

void UsedHeightProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto:
      VisitAuto();
      break;

    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kBlock:
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
