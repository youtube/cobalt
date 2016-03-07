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

#include <vector>

#include "base/optional.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

scoped_refptr<CSSComputedStyleData> PromoteToCascadedStyle(
    const scoped_refptr<const CSSDeclaredStyleData>& inline_style,
    RulesWithCascadePriority* matching_rules,
    GURLMap* property_key_to_base_url_map) {
  scoped_refptr<CSSComputedStyleData> cascaded_style(
      new CSSComputedStyleData());

  // A sparce vector of CascadePriority values for all possible property values.
  base::optional<CascadePriority>
      cascade_precedences[static_cast<size_t>(kNumLonghandProperties)];

  if (inline_style) {
    for (CSSDeclaredStyleData::PropertyValues::const_iterator
             property_value_iterator =
                 inline_style->declared_property_values().begin();
         property_value_iterator !=
         inline_style->declared_property_values().end();
         ++property_value_iterator) {
      const PropertyKey& key = property_value_iterator->first;
      // TODO(***REMOVED***): Verify if we correcly handle '!important' inline styles.
      cascaded_style->SetPropertyValue(key, property_value_iterator->second);
      cascade_precedences[key] = CascadePriority(kImportantMin);
    }
  }

  for (RulesWithCascadePriority::const_iterator rule_iterator =
           matching_rules->begin();
       rule_iterator != matching_rules->end(); ++rule_iterator) {
    const scoped_refptr<const CSSDeclaredStyleData>& declared_style =
        rule_iterator->first->declared_style_data();

    if (declared_style) {
      CascadePriority precedence_normal = rule_iterator->second;
      CascadePriority precedence_important = rule_iterator->second;
      precedence_important.SetImportant();

      for (CSSDeclaredStyleData::PropertyValues::const_iterator
               property_value_iterator =
                   declared_style->declared_property_values().begin();
           property_value_iterator !=
           declared_style->declared_property_values().end();
           ++property_value_iterator) {
        const PropertyKey& key = property_value_iterator->first;
        CascadePriority* precedence =
            declared_style->IsDeclaredPropertyImportant(key)
                ? &precedence_important
                : &precedence_normal;
        if (!(cascade_precedences[key]) ||
            *(cascade_precedences[key]) < *precedence) {
          cascade_precedences[key] = *precedence;
          cascaded_style->SetPropertyValue(key,
                                           property_value_iterator->second);

          if (kBackgroundImageProperty == key) {
            DCHECK(property_key_to_base_url_map);
            (*property_key_to_base_url_map)[kBackgroundImageProperty] =
                rule_iterator->first->parent_style_sheet()->LocationUrl();
          }
        }
      }
    }
  }
  return cascaded_style;
}

}  // namespace cssom
}  // namespace cobalt
