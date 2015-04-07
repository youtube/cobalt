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
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/layout/containing_block.h"

namespace cobalt {
namespace layout {

UsedStyleProvider::UsedStyleProvider(
    render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {}

scoped_refptr<render_tree::Font> UsedStyleProvider::GetUsedFont(
    const scoped_refptr<cssom::PropertyValue>& font_family_refptr,
    const scoped_refptr<cssom::PropertyValue>& font_size_refptr) {
  cssom::StringValue* font_family =
      base::polymorphic_downcast<cssom::StringValue*>(font_family_refptr.get());
  cssom::LengthValue* font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(font_size_refptr.get());
  DCHECK_EQ(cssom::kPixelsUnit, font_size->unit());

  // TODO(***REMOVED***): Implement font style.
  return resource_provider_->GetPreInstalledFont(
      font_family->value().c_str(), render_tree::ResourceProvider::kNormal,
      font_size->value());
}

render_tree::ColorRGBA GetUsedColor(
    const scoped_refptr<cssom::PropertyValue>& color_refptr) {
  cssom::RGBAColorValue* color =
      base::polymorphic_downcast<cssom::RGBAColorValue*>(color_refptr.get());
  return render_tree::ColorRGBA(color->value());
}

UsedHeightProvider::UsedHeightProvider(float total_child_height)
    : total_child_height_(total_child_height) {}

void UsedHeightProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto:
      used_height_ = total_child_height_;
      break;
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void UsedHeightProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit())
      << "TODO(***REMOVED***): Implement other units";
  used_height_ = length->value();
}


UsedWidthProvider::UsedWidthProvider(float total_child_width)
    : total_child_width_(total_child_width) {}

void UsedWidthProvider::VisitKeyword(cssom::KeywordValue* keyword) {
  switch (keyword->value()) {
    case cssom::KeywordValue::kAuto:
      used_width_ = total_child_width_;
      break;
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
  }
}

void UsedWidthProvider::VisitLength(cssom::LengthValue* length) {
  DCHECK_EQ(cssom::kPixelsUnit, length->unit())
      << "TODO(***REMOVED***): Implement other units";
  used_width_ = length->value();
}

}  // namespace layout
}  // namespace cobalt
