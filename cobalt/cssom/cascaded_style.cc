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

#include "cobalt/cssom/cascaded_style.h"

#include <algorithm>

#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {
namespace {

bool CompareRulesWithCascadePriority(const RuleWithCascadePriority& lhs,
                                     const RuleWithCascadePriority& rhs) {
  return lhs.second > rhs.second;
}

}  // namespace

void PromoteToCascadedStyle(const scoped_refptr<CSSStyleDeclarationData>& style,
                            RulesWithCascadePriority* matching_rules,
                            GURLMap* property_name_to_base_url_map) {
  // Sort the matched rules in the order of descending specificity, in other
  // words consider most specific rules first.
  std::sort(matching_rules->begin(), matching_rules->end(),
            CompareRulesWithCascadePriority);

  for (RulesWithCascadePriority::const_iterator rule_iterator =
           matching_rules->begin();
       rule_iterator != matching_rules->end(); ++rule_iterator) {
    scoped_refptr<const CSSStyleDeclarationData> declared_style =
        rule_iterator->first->style()->data();

    // TODO(***REMOVED***): Iterate only over non-null properties in the rule.
    if (!style->background_color() && declared_style->background_color()) {
      style->set_background_color(declared_style->background_color());
    }
    if (!style->background_image() && declared_style->background_image()) {
      style->set_background_image(declared_style->background_image());

      DCHECK(property_name_to_base_url_map);
      (*property_name_to_base_url_map)[kBackgroundImagePropertyName] =
          rule_iterator->first->parent_style_sheet()->LocationUrl();
    }
    if (!style->background_position() &&
        declared_style->background_position()) {
      style->set_background_position(declared_style->background_position());
    }
    if (!style->background_repeat() && declared_style->background_repeat()) {
      style->set_background_repeat(declared_style->background_repeat());
    }
    if (!style->background_size() && declared_style->background_size()) {
      style->set_background_size(declared_style->background_size());
    }
    if (!style->bottom() && declared_style->bottom()) {
      style->set_bottom(declared_style->bottom());
    }
    if (!style->color() && declared_style->color()) {
      style->set_color(declared_style->color());
    }
    if (!style->content() && declared_style->content()) {
      style->set_content(declared_style->content());
    }
    if (!style->display() && declared_style->display()) {
      style->set_display(declared_style->display());
    }
    if (!style->font_family() && declared_style->font_family()) {
      style->set_font_family(declared_style->font_family());
    }
    if (!style->font_size() && declared_style->font_size()) {
      style->set_font_size(declared_style->font_size());
    }
    if (!style->font_style() && declared_style->font_style()) {
      style->set_font_style(declared_style->font_style());
    }
    if (!style->font_weight() && declared_style->font_weight()) {
      style->set_font_weight(declared_style->font_weight());
    }
    if (!style->height() && declared_style->height()) {
      style->set_height(declared_style->height());
    }
    if (!style->left() && declared_style->left()) {
      style->set_left(declared_style->left());
    }
    if (!style->line_height() && declared_style->line_height()) {
      style->set_line_height(declared_style->line_height());
    }
    if (!style->margin_bottom() && declared_style->margin_bottom()) {
      style->set_margin_bottom(declared_style->margin_bottom());
    }
    if (!style->margin_left() && declared_style->margin_left()) {
      style->set_margin_left(declared_style->margin_left());
    }
    if (!style->margin_right() && declared_style->margin_right()) {
      style->set_margin_right(declared_style->margin_right());
    }
    if (!style->margin_top() && declared_style->margin_top()) {
      style->set_margin_top(declared_style->margin_top());
    }
    if (!style->opacity() && declared_style->opacity()) {
      style->set_opacity(declared_style->opacity());
    }
    if (!style->overflow() && declared_style->overflow()) {
      style->set_overflow(declared_style->overflow());
    }
    if (!style->overflow_wrap() && declared_style->overflow_wrap()) {
      style->set_overflow_wrap(declared_style->overflow_wrap());
    }
    if (!style->padding_bottom() && declared_style->padding_bottom()) {
      style->set_padding_bottom(declared_style->padding_bottom());
    }
    if (!style->padding_left() && declared_style->padding_left()) {
      style->set_padding_left(declared_style->padding_left());
    }
    if (!style->padding_right() && declared_style->padding_right()) {
      style->set_padding_right(declared_style->padding_right());
    }
    if (!style->padding_top() && declared_style->padding_top()) {
      style->set_padding_top(declared_style->padding_top());
    }
    if (!style->position() && declared_style->position()) {
      style->set_position(declared_style->position());
    }
    if (!style->right() && declared_style->right()) {
      style->set_right(declared_style->right());
    }
    if (!style->tab_size() && declared_style->tab_size()) {
      style->set_tab_size(declared_style->tab_size());
    }
    if (!style->text_align() && declared_style->text_align()) {
      style->set_text_align(declared_style->text_align());
    }
    if (!style->text_indent() && declared_style->text_indent()) {
      style->set_text_indent(declared_style->text_indent());
    }
    if (!style->text_overflow() && declared_style->text_overflow()) {
      style->set_text_overflow(declared_style->text_overflow());
    }
    if (!style->text_transform() && declared_style->text_transform()) {
      style->set_text_transform(declared_style->text_transform());
    }
    if (!style->top() && declared_style->top()) {
      style->set_top(declared_style->top());
    }
    if (!style->transform() && declared_style->transform()) {
      style->set_transform(declared_style->transform());
    }
    if (!style->transition_delay() && declared_style->transition_delay()) {
      style->set_transition_delay(declared_style->transition_delay());
    }
    if (!style->transition_duration() &&
        declared_style->transition_duration()) {
      style->set_transition_duration(declared_style->transition_duration());
    }
    if (!style->transition_property() &&
        declared_style->transition_property()) {
      style->set_transition_property(declared_style->transition_property());
    }
    if (!style->transition_timing_function() &&
        declared_style->transition_timing_function()) {
      style->set_transition_timing_function(
          declared_style->transition_timing_function());
    }
    if (!style->vertical_align() && declared_style->vertical_align()) {
      style->set_vertical_align(declared_style->vertical_align());
    }
    if (!style->white_space() && declared_style->white_space()) {
      style->set_white_space(declared_style->white_space());
    }
    if (!style->width() && declared_style->width()) {
      style->set_width(declared_style->width());
    }
    if (!style->z_index() && declared_style->z_index()) {
      style->set_z_index(declared_style->z_index());
    }
  }
}

}  // namespace cssom
}  // namespace cobalt
