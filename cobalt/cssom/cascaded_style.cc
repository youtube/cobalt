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

#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

void PromoteToCascadedStyle(const scoped_refptr<CSSStyleDeclarationData>& style,
                            RulesWithCascadePriority* matching_rules,
                            GURLMap* property_name_to_base_url_map) {
  const CascadePriority kPrecedenceNotSet;
  const CascadePriority kPrecedenceImportantMin(kImportantAuthor);

#define DEFINE_PROPERTY_PRECEDENCE(NAME) \
  CascadePriority NAME##_precedence =    \
      style->NAME() ? kPrecedenceImportantMin : kPrecedenceNotSet;

  DEFINE_PROPERTY_PRECEDENCE(background_color)
  DEFINE_PROPERTY_PRECEDENCE(background_image)
  DEFINE_PROPERTY_PRECEDENCE(background_position)
  DEFINE_PROPERTY_PRECEDENCE(background_repeat)
  DEFINE_PROPERTY_PRECEDENCE(background_size)
  DEFINE_PROPERTY_PRECEDENCE(border_radius)
  DEFINE_PROPERTY_PRECEDENCE(bottom)
  DEFINE_PROPERTY_PRECEDENCE(color)
  DEFINE_PROPERTY_PRECEDENCE(content)
  DEFINE_PROPERTY_PRECEDENCE(display)
  DEFINE_PROPERTY_PRECEDENCE(font_family)
  DEFINE_PROPERTY_PRECEDENCE(font_size)
  DEFINE_PROPERTY_PRECEDENCE(font_style)
  DEFINE_PROPERTY_PRECEDENCE(font_weight)
  DEFINE_PROPERTY_PRECEDENCE(height)
  DEFINE_PROPERTY_PRECEDENCE(left)
  DEFINE_PROPERTY_PRECEDENCE(line_height)
  DEFINE_PROPERTY_PRECEDENCE(margin_bottom)
  DEFINE_PROPERTY_PRECEDENCE(margin_left)
  DEFINE_PROPERTY_PRECEDENCE(margin_right)
  DEFINE_PROPERTY_PRECEDENCE(margin_top)
  DEFINE_PROPERTY_PRECEDENCE(max_height)
  DEFINE_PROPERTY_PRECEDENCE(max_width)
  DEFINE_PROPERTY_PRECEDENCE(min_height)
  DEFINE_PROPERTY_PRECEDENCE(min_width)
  DEFINE_PROPERTY_PRECEDENCE(opacity)
  DEFINE_PROPERTY_PRECEDENCE(overflow)
  DEFINE_PROPERTY_PRECEDENCE(overflow_wrap)
  DEFINE_PROPERTY_PRECEDENCE(padding_bottom)
  DEFINE_PROPERTY_PRECEDENCE(padding_left)
  DEFINE_PROPERTY_PRECEDENCE(padding_right)
  DEFINE_PROPERTY_PRECEDENCE(padding_top)
  DEFINE_PROPERTY_PRECEDENCE(position)
  DEFINE_PROPERTY_PRECEDENCE(right)
  DEFINE_PROPERTY_PRECEDENCE(tab_size)
  DEFINE_PROPERTY_PRECEDENCE(text_align)
  DEFINE_PROPERTY_PRECEDENCE(text_indent)
  DEFINE_PROPERTY_PRECEDENCE(text_overflow)
  DEFINE_PROPERTY_PRECEDENCE(text_transform)
  DEFINE_PROPERTY_PRECEDENCE(top)
  DEFINE_PROPERTY_PRECEDENCE(transform)
  DEFINE_PROPERTY_PRECEDENCE(transition_delay)
  DEFINE_PROPERTY_PRECEDENCE(transition_duration)
  DEFINE_PROPERTY_PRECEDENCE(transition_property)
  DEFINE_PROPERTY_PRECEDENCE(transition_timing_function)
  DEFINE_PROPERTY_PRECEDENCE(vertical_align)
  DEFINE_PROPERTY_PRECEDENCE(visibility)
  DEFINE_PROPERTY_PRECEDENCE(white_space)
  DEFINE_PROPERTY_PRECEDENCE(width)
  DEFINE_PROPERTY_PRECEDENCE(z_index)
#undef DEFINE_PROPERTY_PRECEDENCE

  for (RulesWithCascadePriority::const_iterator rule_iterator =
           matching_rules->begin();
       rule_iterator != matching_rules->end(); ++rule_iterator) {
    scoped_refptr<const CSSStyleDeclarationData> declared_style =
        rule_iterator->first->style()->data();

    CascadePriority precedence_normal = rule_iterator->second;
    CascadePriority precedence_important = rule_iterator->second;
    precedence_important.SetImportant();

    // TODO(***REMOVED***): Iterate only over non-null properties in the rule.

#define UPDATE_PROPERTY(NAME, DASHED_NAME)               \
  if (declared_style->NAME()) {                          \
    CascadePriority* precedence =                        \
        declared_style->IsPropertyImportant(DASHED_NAME) \
            ? &precedence_important                      \
            : &precedence_normal;                        \
    if (*precedence > NAME##_precedence) {               \
      NAME##_precedence = *precedence;                   \
      style->set_##NAME(declared_style->NAME());         \
    }                                                    \
  }

    UPDATE_PROPERTY(background_color, kBackgroundColorPropertyName)

    if (declared_style->background_image()) {
      CascadePriority* precedence =
          declared_style->IsPropertyImportant(kBackgroundImagePropertyName)
              ? &precedence_important
              : &precedence_normal;
      if (*precedence > background_image_precedence) {
        background_image_precedence = *precedence;
        style->set_background_image(declared_style->background_image());

        DCHECK(property_name_to_base_url_map);
        (*property_name_to_base_url_map)[kBackgroundImagePropertyName] =
            rule_iterator->first->parent_style_sheet()->LocationUrl();
      }
    }

    UPDATE_PROPERTY(background_position, kBackgroundPositionPropertyName)
    UPDATE_PROPERTY(background_repeat, kBackgroundRepeatPropertyName)
    UPDATE_PROPERTY(background_size, kBackgroundSizePropertyName)
    UPDATE_PROPERTY(border_radius, kBorderRadiusPropertyName)
    UPDATE_PROPERTY(bottom, kBottomPropertyName)
    UPDATE_PROPERTY(color, kColorPropertyName)
    UPDATE_PROPERTY(content, kContentPropertyName)
    UPDATE_PROPERTY(display, kDisplayPropertyName)
    UPDATE_PROPERTY(font_family, kFontFamilyPropertyName)
    UPDATE_PROPERTY(font_size, kFontSizePropertyName)
    UPDATE_PROPERTY(font_style, kFontStylePropertyName)
    UPDATE_PROPERTY(font_weight, kFontWeightPropertyName)
    UPDATE_PROPERTY(height, kHeightPropertyName)
    UPDATE_PROPERTY(left, kLeftPropertyName)
    UPDATE_PROPERTY(line_height, kLineHeightPropertyName)
    UPDATE_PROPERTY(margin_bottom, kMarginBottomPropertyName)
    UPDATE_PROPERTY(margin_left, kMarginLeftPropertyName)
    UPDATE_PROPERTY(margin_right, kMarginRightPropertyName)
    UPDATE_PROPERTY(margin_top, kMarginTopPropertyName)
    UPDATE_PROPERTY(max_height, kMaxHeightPropertyName)
    UPDATE_PROPERTY(max_width, kMaxWidthPropertyName)
    UPDATE_PROPERTY(min_height, kMinHeightPropertyName)
    UPDATE_PROPERTY(min_width, kMinWidthPropertyName)
    UPDATE_PROPERTY(opacity, kOpacityPropertyName)
    UPDATE_PROPERTY(overflow, kOverflowPropertyName)
    UPDATE_PROPERTY(overflow_wrap, kOverflowWrapPropertyName)
    UPDATE_PROPERTY(padding_bottom, kPaddingBottomPropertyName)
    UPDATE_PROPERTY(padding_left, kPaddingLeftPropertyName)
    UPDATE_PROPERTY(padding_right, kPaddingRightPropertyName)
    UPDATE_PROPERTY(padding_top, kPaddingTopPropertyName)
    UPDATE_PROPERTY(position, kPositionPropertyName)
    UPDATE_PROPERTY(right, kRightPropertyName)
    UPDATE_PROPERTY(tab_size, kTabSizePropertyName)
    UPDATE_PROPERTY(text_align, kTextAlignPropertyName)
    UPDATE_PROPERTY(text_indent, kTextIndentPropertyName)
    UPDATE_PROPERTY(text_overflow, kTextOverflowPropertyName)
    UPDATE_PROPERTY(text_transform, kTextTransformPropertyName)
    UPDATE_PROPERTY(top, kTopPropertyName)
    UPDATE_PROPERTY(transform, kTransformPropertyName)
    UPDATE_PROPERTY(transition_delay, kTransitionDelayPropertyName)
    UPDATE_PROPERTY(transition_duration, kTransitionDurationPropertyName)
    UPDATE_PROPERTY(transition_property, kTransitionPropertyPropertyName)
    UPDATE_PROPERTY(transition_timing_function,
                    kTransitionTimingFunctionPropertyName)
    UPDATE_PROPERTY(vertical_align, kVerticalAlignPropertyName)
    UPDATE_PROPERTY(visibility, kVisibilityPropertyName)
    UPDATE_PROPERTY(white_space, kWhiteSpacePropertyName)
    UPDATE_PROPERTY(width, kWidthPropertyName)
    UPDATE_PROPERTY(z_index, kZIndexPropertyName)
#undef UPDATE_PROPERTY
  }
}

}  // namespace cssom
}  // namespace cobalt
