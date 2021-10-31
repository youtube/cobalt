// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/cascaded_style.h"

#include <vector>

#include "base/optional.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

namespace {

void SetPropertyValuesOfHigherPrecedence(
    const scoped_refptr<const CSSDeclaredStyleData>& style,
    const CascadePrecedence& precedence_normal,
    const CascadePrecedence& precedence_important,
    base::Optional<CascadePrecedence>* cascade_precedences,
    scoped_refptr<MutableCSSComputedStyleData>* cascaded_style,
    bool* background_image_refreshed) {
  const CSSDeclaredStyleData::PropertyValues& property_values =
      style->declared_property_values();
  for (CSSDeclaredStyleData::PropertyValues::const_iterator
           property_value_iterator = property_values.begin();
       property_value_iterator != property_values.end();
       ++property_value_iterator) {
    const PropertyKey& key = property_value_iterator->first;
    DCHECK_GT(key, kNoneProperty);
    DCHECK_LE(key, kMaxLonghandPropertyKey);

    const CascadePrecedence& precedence =
        style->IsDeclaredPropertyImportant(key) ? precedence_important
                                                : precedence_normal;
    if (!(cascade_precedences[key]) ||
        *(cascade_precedences[key]) < precedence) {
      cascade_precedences[key] = precedence;
      (*cascaded_style)->SetPropertyValue(key, property_value_iterator->second);
      if (kBackgroundImageProperty == key) {
        *background_image_refreshed = true;
      }
    }
  }
}

}  // namespace

scoped_refptr<MutableCSSComputedStyleData> PromoteToCascadedStyle(
    const scoped_refptr<const CSSDeclaredStyleData>& inline_style,
    RulesWithCascadePrecedence* matching_rules,
    GURLMap* property_key_to_base_url_map) {
  scoped_refptr<MutableCSSComputedStyleData> cascaded_style(
      new MutableCSSComputedStyleData());

  // A sparse vector of CascadePrecedence values for all possible property
  // values.
  base::Optional<CascadePrecedence>
      cascade_precedences[static_cast<size_t>(kNumLonghandProperties)];

  if (inline_style) {
    const CascadePrecedence precedence_normal =
        CascadePrecedence(kImportantMin);
    const CascadePrecedence precedence_important =
        CascadePrecedence(kImportantMax);
    bool background_image_refreshed = false;
    SetPropertyValuesOfHigherPrecedence(
        inline_style, precedence_normal, precedence_important,
        cascade_precedences, &cascaded_style, &background_image_refreshed);
  }

  if (!matching_rules->empty()) {
    for (RulesWithCascadePrecedence::const_iterator rule_iterator =
             matching_rules->begin();
         rule_iterator != matching_rules->end(); ++rule_iterator) {
      const scoped_refptr<const CSSDeclaredStyleData>& declared_style =
          rule_iterator->first->declared_style_data();
      if (declared_style) {
        const CascadePrecedence& precedence_normal = rule_iterator->second;
        CascadePrecedence precedence_important = rule_iterator->second;
        precedence_important.SetImportant();
        bool background_image_refreshed = false;
        SetPropertyValuesOfHigherPrecedence(
            declared_style, precedence_normal, precedence_important,
            cascade_precedences, &cascaded_style, &background_image_refreshed);

        if (background_image_refreshed) {
          const scoped_refptr<CSSStyleSheet>& parent_style_sheet =
              rule_iterator->first->parent_style_sheet();
          if (parent_style_sheet &&
              parent_style_sheet->LocationUrl().is_valid()) {
            DCHECK(property_key_to_base_url_map);
            (*property_key_to_base_url_map)[kBackgroundImageProperty] =
                parent_style_sheet->LocationUrl();
          }
        }
      }
    }
  }
  return cascaded_style;
}

}  // namespace cssom
}  // namespace cobalt
