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

#include "cobalt/cssom/css_style_declaration.h"

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_declaration_util.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/property_names.h"

namespace cobalt {
namespace cssom {

namespace {

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : location(base::SourceLocation("[object CSSStyleDeclaration]", 1, 1)) {}

  const base::SourceLocation location;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

CSSStyleDeclaration::CSSStyleDeclaration(CSSParser* css_parser)
    : data_(new CSSStyleDeclarationData),
      css_parser_(css_parser),
      mutation_observer_(NULL) {}

CSSStyleDeclaration::CSSStyleDeclaration(
    const scoped_refptr<CSSStyleDeclarationData>& style, CSSParser* css_parser)
    : data_(style), css_parser_(css_parser), mutation_observer_(NULL) {
  DCHECK(data_.get());
}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

std::string CSSStyleDeclaration::background() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other background properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_background(const std::string& background) {
  SetPropertyValue(kBackgroundPropertyName, background);
}

std::string CSSStyleDeclaration::background_color() const {
  return data_->background_color() ? data_->background_color()->ToString() : "";
}

void CSSStyleDeclaration::set_background_color(
    const std::string& background_color) {
  SetPropertyValue(kBackgroundColorPropertyName, background_color);
}

std::string CSSStyleDeclaration::background_image() const {
  return data_->background_image() ? data_->background_image()->ToString() : "";
}

void CSSStyleDeclaration::set_background_image(
    const std::string& background_image) {
  SetPropertyValue(kBackgroundImagePropertyName, background_image);
}

std::string CSSStyleDeclaration::background_position() const {
  return data_->background_position() ? data_->background_position()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_background_position(
    const std::string& background_position) {
  SetPropertyValue(kBackgroundPositionPropertyName, background_position);
}

std::string CSSStyleDeclaration::background_repeat() const {
  return data_->background_repeat() ? data_->background_repeat()->ToString()
                                    : "";
}

void CSSStyleDeclaration::set_background_repeat(
    const std::string& background_repeat) {
  SetPropertyValue(kBackgroundRepeatPropertyName, background_repeat);
}

std::string CSSStyleDeclaration::background_size() const {
  return data_->background_size() ? data_->background_size()->ToString() : "";
}

void CSSStyleDeclaration::set_background_size(
    const std::string& background_size) {
  SetPropertyValue(kBackgroundSizePropertyName, background_size);
}

std::string CSSStyleDeclaration::border_radius() const {
  return data_->border_radius() ? data_->border_radius()->ToString() : "";
}

void CSSStyleDeclaration::set_border_radius(const std::string& border_radius) {
  SetPropertyValue(kBorderRadiusPropertyName, border_radius);
}

std::string CSSStyleDeclaration::bottom() const {
  return data_->bottom() ? data_->bottom()->ToString() : "";
}

void CSSStyleDeclaration::set_bottom(const std::string& bottom) {
  SetPropertyValue(kBottomPropertyName, bottom);
}

std::string CSSStyleDeclaration::color() const {
  return data_->color() ? data_->color()->ToString() : "";
}

void CSSStyleDeclaration::set_color(const std::string& color) {
  SetPropertyValue(kColorPropertyName, color);
}

std::string CSSStyleDeclaration::content() const {
  return data_->content() ? data_->content()->ToString() : "";
}

void CSSStyleDeclaration::set_content(const std::string& content) {
  SetPropertyValue(kContentPropertyName, content);
}

std::string CSSStyleDeclaration::display() const {
  return data_->display() ? data_->display()->ToString() : "";
}

void CSSStyleDeclaration::set_display(const std::string& display) {
  SetPropertyValue(kDisplayPropertyName, display);
}

std::string CSSStyleDeclaration::font_family() const {
  return data_->font_family() ? data_->font_family()->ToString() : "";
}

void CSSStyleDeclaration::set_font_family(const std::string& font_family) {
  SetPropertyValue(kFontFamilyPropertyName, font_family);
}

std::string CSSStyleDeclaration::font_size() const {
  return data_->font_size() ? data_->font_size()->ToString() : "";
}

void CSSStyleDeclaration::set_font_size(const std::string& font_size) {
  SetPropertyValue(kFontSizePropertyName, font_size);
}

std::string CSSStyleDeclaration::font_style() const {
  return data_->font_style() ? data_->font_style()->ToString() : "";
}

void CSSStyleDeclaration::set_font_style(const std::string& font_style) {
  SetPropertyValue(kFontStylePropertyName, font_style);
}

std::string CSSStyleDeclaration::font_weight() const {
  return data_->font_weight() ? data_->font_weight()->ToString() : "";
}

void CSSStyleDeclaration::set_font_weight(const std::string& font_weight) {
  SetPropertyValue(kFontWeightPropertyName, font_weight);
}

std::string CSSStyleDeclaration::height() const {
  return data_->height() ? data_->height()->ToString() : "";
}

void CSSStyleDeclaration::set_height(const std::string& height) {
  SetPropertyValue(kHeightPropertyName, height);
}

std::string CSSStyleDeclaration::left() const {
  return data_->left() ? data_->left()->ToString() : "";
}

void CSSStyleDeclaration::set_left(const std::string& left) {
  SetPropertyValue(kLeftPropertyName, left);
}

std::string CSSStyleDeclaration::line_height() const {
  return data_->line_height() ? data_->line_height()->ToString() : "";
}

void CSSStyleDeclaration::set_line_height(const std::string& line_height) {
  SetPropertyValue(kLineHeightPropertyName, line_height);
}

std::string CSSStyleDeclaration::max_height() const {
  return data_->max_height() ? data_->max_height()->ToString() : "";
}

void CSSStyleDeclaration::set_max_height(const std::string& max_height) {
  SetPropertyValue(kMaxHeightPropertyName, max_height);
}

std::string CSSStyleDeclaration::max_width() const {
  return data_->max_width() ? data_->max_width()->ToString() : "";
}

void CSSStyleDeclaration::set_max_width(const std::string& max_width) {
  SetPropertyValue(kMaxWidthPropertyName, max_width);
}

std::string CSSStyleDeclaration::min_height() const {
  return data_->min_height() ? data_->min_height()->ToString() : "";
}

void CSSStyleDeclaration::set_min_height(const std::string& min_height) {
  SetPropertyValue(kMinHeightPropertyName, min_height);
}

std::string CSSStyleDeclaration::min_width() const {
  return data_->min_width() ? data_->min_width()->ToString() : "";
}

void CSSStyleDeclaration::set_min_width(const std::string& min_width) {
  SetPropertyValue(kMinWidthPropertyName, min_width);
}

std::string CSSStyleDeclaration::opacity() const {
  return data_->opacity() ? data_->opacity()->ToString() : "";
}

void CSSStyleDeclaration::set_opacity(const std::string& opacity) {
  SetPropertyValue(kOpacityPropertyName, opacity);
}

std::string CSSStyleDeclaration::overflow() const {
  return data_->overflow() ? data_->overflow()->ToString() : "";
}

void CSSStyleDeclaration::set_overflow(const std::string& overflow) {
  SetPropertyValue(kOverflowPropertyName, overflow);
}

std::string CSSStyleDeclaration::overflow_wrap() const {
  return data_->overflow_wrap() ? data_->overflow_wrap()->ToString() : "";
}

void CSSStyleDeclaration::set_overflow_wrap(const std::string& overflow_wrap) {
  SetPropertyValue(kOverflowWrapPropertyName, overflow_wrap);
}

std::string CSSStyleDeclaration::position() const {
  return data_->position() ? data_->position()->ToString() : "";
}

void CSSStyleDeclaration::set_position(const std::string& position) {
  SetPropertyValue(kPositionPropertyName, position);
}

std::string CSSStyleDeclaration::right() const {
  return data_->right() ? data_->right()->ToString() : "";
}

void CSSStyleDeclaration::set_right(const std::string& right) {
  SetPropertyValue(kRightPropertyName, right);
}

std::string CSSStyleDeclaration::tab_size() const {
  return data_->tab_size() ? data_->tab_size()->ToString() : "";
}

void CSSStyleDeclaration::set_tab_size(const std::string& tab_size) {
  SetPropertyValue(kTabSizePropertyName, tab_size);
}

std::string CSSStyleDeclaration::text_align() const {
  return data_->text_align() ? data_->text_align()->ToString() : "";
}

void CSSStyleDeclaration::set_text_align(const std::string& text_align) {
  SetPropertyValue(kTextAlignPropertyName, text_align);
}

std::string CSSStyleDeclaration::text_indent() const {
  return data_->text_indent() ? data_->text_indent()->ToString() : "";
}

void CSSStyleDeclaration::set_text_indent(const std::string& text_indent) {
  SetPropertyValue(kTextIndentPropertyName, text_indent);
}

std::string CSSStyleDeclaration::text_overflow() const {
  return data_->text_overflow() ? data_->text_overflow()->ToString() : "";
}

void CSSStyleDeclaration::set_text_overflow(const std::string& text_overflow) {
  SetPropertyValue(kTextOverflowPropertyName, text_overflow);
}

std::string CSSStyleDeclaration::text_transform() const {
  return data_->text_transform() ? data_->text_transform()->ToString() : "";
}

void CSSStyleDeclaration::set_text_transform(
    const std::string& text_transform) {
  SetPropertyValue(kTextTransformPropertyName, text_transform);
}

std::string CSSStyleDeclaration::top() const {
  return data_->top() ? data_->top()->ToString() : "";
}

void CSSStyleDeclaration::set_top(const std::string& top) {
  SetPropertyValue(kTopPropertyName, top);
}

std::string CSSStyleDeclaration::transform() const {
  return data_->transform() ? data_->transform()->ToString() : "";
}

void CSSStyleDeclaration::set_transform(const std::string& transform) {
  SetPropertyValue(kTransformPropertyName, transform);
}

std::string CSSStyleDeclaration::transition() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other transition properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_transition(const std::string& transition) {
  SetPropertyValue(kTransitionPropertyName, transition);
}

std::string CSSStyleDeclaration::transition_delay() const {
  return data_->transition_delay() ? data_->transition_delay()->ToString() : "";
}

void CSSStyleDeclaration::set_transition_delay(
    const std::string& transition_delay) {
  SetPropertyValue(kTransitionDelayPropertyName, transition_delay);
}

std::string CSSStyleDeclaration::transition_duration() const {
  return data_->transition_duration() ? data_->transition_duration()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_duration(
    const std::string& transition_duration) {
  SetPropertyValue(kTransitionDurationPropertyName, transition_duration);
}

std::string CSSStyleDeclaration::transition_property() const {
  return data_->transition_property() ? data_->transition_property()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_property(
    const std::string& transition_property) {
  SetPropertyValue(kTransitionPropertyPropertyName, transition_property);
}

std::string CSSStyleDeclaration::transition_timing_function() const {
  return data_->transition_timing_function()
             ? data_->transition_timing_function()->ToString()
             : "";
}

void CSSStyleDeclaration::set_transition_timing_function(
    const std::string& transition_timing_function) {
  SetPropertyValue(kTransitionTimingFunctionPropertyName,
                   transition_timing_function);
}

std::string CSSStyleDeclaration::vertical_align() const {
  return data_->vertical_align() ? data_->vertical_align()->ToString() : "";
}

void CSSStyleDeclaration::set_vertical_align(
    const std::string& vertical_align) {
  SetPropertyValue(kVerticalAlignPropertyName, vertical_align);
}

std::string CSSStyleDeclaration::visibility() const {
  return data_->visibility() ? data_->visibility()->ToString() : "";
}

void CSSStyleDeclaration::set_visibility(const std::string& visibility) {
  SetPropertyValue(kVisibilityPropertyName, visibility);
}

std::string CSSStyleDeclaration::white_space() const {
  return data_->white_space() ? data_->white_space()->ToString() : "";
}

void CSSStyleDeclaration::set_white_space(const std::string& white_space) {
  SetPropertyValue(kWhiteSpacePropertyName, white_space);
}

std::string CSSStyleDeclaration::width() const {
  return data_->width() ? data_->width()->ToString() : "";
}

void CSSStyleDeclaration::set_width(const std::string& width) {
  SetPropertyValue(kWidthPropertyName, width);
}

// word-wrap is treated as an alias for overflow-wrap
//   http://www.w3.org/TR/css-text-3/#overflow-wrap
std::string CSSStyleDeclaration::word_wrap() const {
  return data_->overflow_wrap() ? data_->overflow_wrap()->ToString() : "";
}

void CSSStyleDeclaration::set_word_wrap(const std::string& word_wrap) {
  SetPropertyValue(kWordWrapPropertyName, word_wrap);
}

std::string CSSStyleDeclaration::z_index() const {
  return data_->z_index() ? data_->z_index()->ToString() : "";
}

void CSSStyleDeclaration::set_z_index(const std::string& z_index) {
  SetPropertyValue(kZIndexPropertyName, z_index);
}

// This returns the result of serializing a CSS declaration block.
// The current implementation does not handle shorthands.
//   http://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
std::string CSSStyleDeclaration::css_text() const {
  std::string css_text;
  AppendPropertyDeclaration(kBackgroundColorPropertyName,
                            data_->background_color(), &css_text);
  AppendPropertyDeclaration(kBackgroundImagePropertyName,
                            data_->background_image(), &css_text);
  AppendPropertyDeclaration(kBackgroundPositionPropertyName,
                            data_->background_position(), &css_text);
  AppendPropertyDeclaration(kBackgroundRepeatPropertyName,
                            data_->background_repeat(), &css_text);
  AppendPropertyDeclaration(kBackgroundSizePropertyName,
                            data_->background_size(), &css_text);
  AppendPropertyDeclaration(kBorderRadiusPropertyName, data_->border_radius(),
                            &css_text);
  AppendPropertyDeclaration(kBottomPropertyName, data_->bottom(), &css_text);
  AppendPropertyDeclaration(kColorPropertyName, data_->color(), &css_text);
  AppendPropertyDeclaration(kContentPropertyName, data_->content(), &css_text);
  AppendPropertyDeclaration(kDisplayPropertyName, data_->display(), &css_text);
  AppendPropertyDeclaration(kFontFamilyPropertyName, data_->font_family(),
                            &css_text);
  AppendPropertyDeclaration(kFontSizePropertyName, data_->font_size(),
                            &css_text);
  AppendPropertyDeclaration(kFontStylePropertyName, data_->font_style(),
                            &css_text);
  AppendPropertyDeclaration(kFontWeightPropertyName, data_->font_weight(),
                            &css_text);
  AppendPropertyDeclaration(kHeightPropertyName, data_->height(), &css_text);
  AppendPropertyDeclaration(kLeftPropertyName, data_->left(), &css_text);
  AppendPropertyDeclaration(kLineHeightPropertyName, data_->line_height(),
                            &css_text);
  AppendPropertyDeclaration(kMarginBottomPropertyName, data_->margin_bottom(),
                            &css_text);
  AppendPropertyDeclaration(kMarginLeftPropertyName, data_->margin_left(),
                            &css_text);
  AppendPropertyDeclaration(kMarginRightPropertyName, data_->margin_right(),
                            &css_text);
  AppendPropertyDeclaration(kMarginTopPropertyName, data_->margin_top(),
                            &css_text);
  AppendPropertyDeclaration(kOpacityPropertyName, data_->opacity(), &css_text);
  AppendPropertyDeclaration(kOverflowPropertyName, data_->overflow(),
                            &css_text);
  AppendPropertyDeclaration(kOverflowWrapPropertyName, data_->overflow_wrap(),
                            &css_text);
  AppendPropertyDeclaration(kPaddingBottomPropertyName, data_->padding_bottom(),
                            &css_text);
  AppendPropertyDeclaration(kPaddingLeftPropertyName, data_->padding_left(),
                            &css_text);
  AppendPropertyDeclaration(kPaddingRightPropertyName, data_->padding_right(),
                            &css_text);
  AppendPropertyDeclaration(kPaddingTopPropertyName, data_->padding_top(),
                            &css_text);
  AppendPropertyDeclaration(kPositionPropertyName, data_->position(),
                            &css_text);
  AppendPropertyDeclaration(kRightPropertyName, data_->right(), &css_text);
  AppendPropertyDeclaration(kTabSizePropertyName, data_->tab_size(), &css_text);
  AppendPropertyDeclaration(kTextAlignPropertyName, data_->text_align(),
                            &css_text);
  AppendPropertyDeclaration(kTextIndentPropertyName, data_->text_indent(),
                            &css_text);
  AppendPropertyDeclaration(kTextOverflowPropertyName, data_->text_overflow(),
                            &css_text);
  AppendPropertyDeclaration(kTextTransformPropertyName, data_->text_transform(),
                            &css_text);
  AppendPropertyDeclaration(kTopPropertyName, data_->top(), &css_text);
  AppendPropertyDeclaration(kTransformPropertyName, data_->transform(),
                            &css_text);
  AppendPropertyDeclaration(kTransitionDelayPropertyName,
                            data_->transition_delay(), &css_text);
  AppendPropertyDeclaration(kTransitionDurationPropertyName,
                            data_->transition_duration(), &css_text);
  AppendPropertyDeclaration(kTransitionPropertyPropertyName,
                            data_->transition_property(), &css_text);
  AppendPropertyDeclaration(kTransitionTimingFunctionPropertyName,
                            data_->transition_timing_function(), &css_text);
  AppendPropertyDeclaration(kVerticalAlignPropertyName, data_->vertical_align(),
                            &css_text);
  AppendPropertyDeclaration(kWhiteSpacePropertyName, data_->white_space(),
                            &css_text);
  AppendPropertyDeclaration(kWidthPropertyName, data_->width(), &css_text);
  AppendPropertyDeclaration(kZIndexPropertyName, data_->z_index(), &css_text);

  return css_text;
}

void CSSStyleDeclaration::set_css_text(const std::string& css_text) {
  DCHECK(css_parser_);
  scoped_refptr<CSSStyleDeclarationData> declaration =
      css_parser_->ParseStyleDeclarationList(
          css_text, non_trivial_static_fields.Get().location);

  if (declaration) {
    data_ = declaration;
  } else {
    data_ = new CSSStyleDeclarationData();
  }

  RecordMutation();
}

// Algorithm for length:
//   http://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
unsigned int CSSStyleDeclaration::length() const {
  unsigned int length = 0;

  if (data_->background_color()) {
    ++length;
  }
  if (data_->background_image()) {
    ++length;
  }
  if (data_->background_position()) {
    ++length;
  }
  if (data_->background_repeat()) {
    ++length;
  }
  if (data_->background_size()) {
    ++length;
  }
  if (data_->border_radius()) {
    ++length;
  }
  if (data_->bottom()) {
    ++length;
  }
  if (data_->color()) {
    ++length;
  }
  if (data_->content()) {
    ++length;
  }
  if (data_->display()) {
    ++length;
  }
  if (data_->font_family()) {
    ++length;
  }
  if (data_->font_size()) {
    ++length;
  }
  if (data_->font_style()) {
    ++length;
  }
  if (data_->font_weight()) {
    ++length;
  }
  if (data_->height()) {
    ++length;
  }
  if (data_->left()) {
    ++length;
  }
  if (data_->line_height()) {
    ++length;
  }
  if (data_->margin_bottom()) {
    ++length;
  }
  if (data_->margin_left()) {
    ++length;
  }
  if (data_->margin_right()) {
    ++length;
  }
  if (data_->margin_top()) {
    ++length;
  }
  if (data_->opacity()) {
    ++length;
  }
  if (data_->overflow()) {
    ++length;
  }
  if (data_->overflow_wrap()) {
    ++length;
  }
  if (data_->padding_bottom()) {
    ++length;
  }
  if (data_->padding_left()) {
    ++length;
  }
  if (data_->padding_right()) {
    ++length;
  }
  if (data_->padding_top()) {
    ++length;
  }
  if (data_->position()) {
    ++length;
  }
  if (data_->right()) {
    ++length;
  }
  if (data_->tab_size()) {
    ++length;
  }
  if (data_->text_align()) {
    ++length;
  }
  if (data_->text_indent()) {
    ++length;
  }
  if (data_->text_overflow()) {
    ++length;
  }
  if (data_->text_transform()) {
    ++length;
  }
  if (data_->top()) {
    ++length;
  }
  if (data_->transform()) {
    ++length;
  }
  if (data_->transition_delay()) {
    ++length;
  }
  if (data_->transition_duration()) {
    ++length;
  }
  if (data_->transition_property()) {
    ++length;
  }
  if (data_->transition_timing_function()) {
    ++length;
  }
  if (data_->vertical_align()) {
    ++length;
  }
  if (data_->white_space()) {
    ++length;
  }
  if (data_->width()) {
    ++length;
  }
  if (data_->z_index()) {
    ++length;
  }
  return length;
}

// Algorithm for Item:
//   http://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
base::optional<std::string> CSSStyleDeclaration::Item(
    unsigned int index) const {
  unsigned int length = 0;

  if (data_->background_color()) {
    if (length == index) {
      return std::string(kBackgroundColorPropertyName);
    }
    ++length;
  }
  if (data_->background_image()) {
    if (length == index) {
      return std::string(kBackgroundImagePropertyName);
    }
    ++length;
  }
  if (data_->background_position()) {
    if (length == index) {
      return std::string(kBackgroundPositionPropertyName);
    }
    ++length;
  }
  if (data_->background_repeat()) {
    if (length == index) {
      return std::string(kBackgroundRepeatPropertyName);
    }
    ++length;
  }
  if (data_->background_size()) {
    if (length == index) {
      return std::string(kBackgroundSizePropertyName);
    }
    ++length;
  }
  if (data_->border_radius()) {
    if (length == index) {
      return std::string(kBorderRadiusPropertyName);
    }
    ++length;
  }
  if (data_->bottom()) {
    if (length == index) {
      return std::string(kBottomPropertyName);
    }
    ++length;
  }
  if (data_->color()) {
    if (length == index) {
      return std::string(kColorPropertyName);
    }
    ++length;
  }
  if (data_->content()) {
    if (length == index) {
      return std::string(kContentPropertyName);
    }
    ++length;
  }
  if (data_->display()) {
    if (length == index) {
      return std::string(kDisplayPropertyName);
    }
    ++length;
  }
  if (data_->font_family()) {
    if (length == index) {
      return std::string(kFontFamilyPropertyName);
    }
    ++length;
  }
  if (data_->font_size()) {
    if (length == index) {
      return std::string(kFontSizePropertyName);
    }
    ++length;
  }
  if (data_->font_style()) {
    if (length == index) {
      return std::string(kFontStylePropertyName);
    }
    ++length;
  }
  if (data_->font_weight()) {
    if (length == index) {
      return std::string(kFontWeightPropertyName);
    }
    ++length;
  }
  if (data_->height()) {
    if (length == index) {
      return std::string(kHeightPropertyName);
    }
    ++length;
  }
  if (data_->left()) {
    if (length == index) {
      return std::string(kLeftPropertyName);
    }
    ++length;
  }
  if (data_->line_height()) {
    if (length == index) {
      return std::string(kLineHeightPropertyName);
    }
    ++length;
  }
  if (data_->margin_bottom()) {
    if (length == index) {
      return std::string(kMarginBottomPropertyName);
    }
    ++length;
  }
  if (data_->margin_left()) {
    if (length == index) {
      return std::string(kMarginLeftPropertyName);
    }
    ++length;
  }
  if (data_->margin_right()) {
    if (length == index) {
      return std::string(kMarginRightPropertyName);
    }
    ++length;
  }
  if (data_->margin_top()) {
    if (length == index) {
      return std::string(kMarginTopPropertyName);
    }
    ++length;
  }
  if (data_->opacity()) {
    if (length == index) {
      return std::string(kOpacityPropertyName);
    }
    ++length;
  }
  if (data_->overflow()) {
    if (length == index) {
      return std::string(kOverflowPropertyName);
    }
    ++length;
  }
  if (data_->overflow_wrap()) {
    if (length == index) {
      return std::string(kOverflowWrapPropertyName);
    }
    ++length;
  }
  if (data_->padding_bottom()) {
    if (length == index) {
      return std::string(kPaddingBottomPropertyName);
    }
    ++length;
  }
  if (data_->padding_left()) {
    if (length == index) {
      return std::string(kPaddingLeftPropertyName);
    }
    ++length;
  }
  if (data_->padding_right()) {
    if (length == index) {
      return std::string(kPaddingRightPropertyName);
    }
    ++length;
  }
  if (data_->padding_top()) {
    if (length == index) {
      return std::string(kPaddingTopPropertyName);
    }
    ++length;
  }
  if (data_->position()) {
    if (length == index) {
      return std::string(kPositionPropertyName);
    }
    ++length;
  }
  if (data_->right()) {
    if (length == index) {
      return std::string(kRightPropertyName);
    }
    ++length;
  }
  if (data_->tab_size()) {
    if (length == index) {
      return std::string(kTabSizePropertyName);
    }
    ++length;
  }
  if (data_->text_align()) {
    if (length == index) {
      return std::string(kTextAlignPropertyName);
    }
    ++length;
  }
  if (data_->text_indent()) {
    if (length == index) {
      return std::string(kTextIndentPropertyName);
    }
    ++length;
  }
  if (data_->text_overflow()) {
    if (length == index) {
      return std::string(kTextOverflowPropertyName);
    }
    ++length;
  }
  if (data_->text_transform()) {
    if (length == index) {
      return std::string(kTextTransformPropertyName);
    }
    ++length;
  }
  if (data_->top()) {
    if (length == index) {
      return std::string(kTopPropertyName);
    }
    ++length;
  }
  if (data_->transform()) {
    if (length == index) {
      return std::string(kTransformPropertyName);
    }
    ++length;
  }
  if (data_->transition_delay()) {
    if (length == index) {
      return std::string(kTransitionDelayPropertyName);
    }
    ++length;
  }
  if (data_->transition_duration()) {
    if (length == index) {
      return std::string(kTransitionDurationPropertyName);
    }
    ++length;
  }
  if (data_->transition_property()) {
    if (length == index) {
      return std::string(kTransitionPropertyPropertyName);
    }
    ++length;
  }
  if (data_->transition_timing_function()) {
    if (length == index) {
      return std::string(kTransitionTimingFunctionPropertyName);
    }
    ++length;
  }
  if (data_->vertical_align()) {
    if (length == index) {
      return std::string(kVerticalAlignPropertyName);
    }
    ++length;
  }
  if (data_->white_space()) {
    if (length == index) {
      return std::string(kWhiteSpacePropertyName);
    }
    ++length;
  }
  if (data_->width()) {
    if (length == index) {
      return std::string(kWidthPropertyName);
    }
    ++length;
  }
  if (data_->z_index()) {
    if (length == index) {
      return std::string(kZIndexPropertyName);
    }
  }

  return base::nullopt;
}

std::string CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  return data_->GetPropertyValue(property_name)
             ? data_->GetPropertyValue(property_name)->ToString()
             : "";
}

void CSSStyleDeclaration::SetPropertyValue(const std::string& property_name,
                                           const std::string& property_value) {
  DCHECK(css_parser_);
  css_parser_->ParsePropertyIntoDeclarationData(
      property_name, property_value, non_trivial_static_fields.Get().location,
      data_.get());

  RecordMutation();
}

void CSSStyleDeclaration::RecordMutation() {
  if (mutation_observer_) {
    // Trigger layout update.
    mutation_observer_->OnCSSMutation();
  }
}

}  // namespace cssom
}  // namespace cobalt
