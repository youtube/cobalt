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
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

void PromoteToCascadedStyle(const scoped_refptr<CSSStyleDeclarationData>& style,
                            RulesWithCascadePriority* matching_rules,
                            GURLMap* property_key_to_base_url_map) {
  // A sparce vector of CascadePriority values for all possible property values.
  base::optional<CascadePriority>
      cascade_precedences[static_cast<size_t>(kNumLonghandProperties)];

  for (CSSStyleDeclarationData::PropertyValueConstIterator
           inline_property_value_iterator =
               style->BeginPropertyValueConstIterator();
       !inline_property_value_iterator.Done();
       inline_property_value_iterator.Next()) {
    PropertyKey key = inline_property_value_iterator.Key();
    DCHECK_GT(key, kNoneProperty);
    DCHECK_LE(key, kMaxLonghandPropertyKey);
    // TODO(***REMOVED***): Verify if we correcly handle '!important' inline styles.
    cascade_precedences[key] = CascadePriority(kImportantMin);
  }

  for (RulesWithCascadePriority::const_iterator rule_iterator =
           matching_rules->begin();
       rule_iterator != matching_rules->end(); ++rule_iterator) {
    scoped_refptr<const CSSStyleDeclarationData> declared_style =
        rule_iterator->first->style()->data();

    CascadePriority precedence_normal = rule_iterator->second;
    CascadePriority precedence_important = rule_iterator->second;
    precedence_important.SetImportant();

    for (CSSStyleDeclarationData::PropertyValueConstIterator
             property_value_iterator =
                 declared_style->BeginPropertyValueConstIterator();
         !property_value_iterator.Done(); property_value_iterator.Next()) {
      PropertyKey key = property_value_iterator.Key();
      DCHECK_GT(key, kNoneProperty);
      DCHECK_LE(key, kMaxLonghandPropertyKey);

      CascadePriority* precedence =
          declared_style->IsDeclaredPropertyImportant(key)
              ? &precedence_important
              : &precedence_normal;
      if (!(cascade_precedences[key]) ||
          *(cascade_precedences[key]) < *precedence) {
        cascade_precedences[key] = *precedence;
        style->SetPropertyValue(key, property_value_iterator.ConstValue());

        if (kBackgroundImageProperty == key) {
          DCHECK(property_key_to_base_url_map);
          (*property_key_to_base_url_map)[kBackgroundImageProperty] =
              rule_iterator->first->parent_style_sheet()->LocationUrl();
        }
      }
    }
  }
}

}  // namespace cssom
}  // namespace cobalt
