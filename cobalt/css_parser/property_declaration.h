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

#ifndef COBALT_CSS_PARSER_PROPERTY_DECLARATION_H_
#define COBALT_CSS_PARSER_PROPERTY_DECLARATION_H_

#include <vector>

#include "cobalt/css_parser/trivial_string_piece.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace css_parser {

// A helper used in YYSTYPE to hold together the name, value, and importance
// of the property.  Note that data is stored as a list of name/value pairs
// because shorthand properties (e.g. 'transition') may result in multiple
// non-shorthand properties being declared simultaneously.
struct PropertyDeclaration {
  // Shortcut constructor for when there is only one name/value pair to set.
  // This will be used for the case of non-shorthand properties.
  PropertyDeclaration(cssom::PropertyKey key,
                      const scoped_refptr<cssom::PropertyValue>& value,
                      bool is_important)
      : is_important(is_important) {
    property_values.push_back(PropertyKeyValuePair(key, value));
  }

  // Shorthand properties will construct an empty PropertyDeclaration object
  // using this constructor object and then populate it manually.
  explicit PropertyDeclaration(bool is_important)
      : is_important(is_important) {}

  void Apply(cssom::CSSDeclarationData* declaration_data) {
    for (PropertyKeyValuePairList::const_iterator iter =
             property_values.begin();
         iter != property_values.end(); ++iter) {
      if (declaration_data->IsSupportedPropertyKey(iter->key)) {
        declaration_data->SetPropertyValueAndImportance(iter->key, iter->value,
                                                        is_important);
      } else {
        unsupported_property_keys.push_back(iter->key);
      }
    }
  }

  struct PropertyKeyValuePair {
    PropertyKeyValuePair(cssom::PropertyKey key,
                         const scoped_refptr<cssom::PropertyValue>& value)
        : key(key), value(value) {}

    cssom::PropertyKey key;
    scoped_refptr<cssom::PropertyValue> value;
  };
  typedef std::vector<PropertyKeyValuePair> PropertyKeyValuePairList;
  typedef std::vector<cssom::PropertyKey> PropertyKeyList;

  PropertyKeyValuePairList property_values;
  bool is_important;
  PropertyKeyList unsupported_property_keys;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_PROPERTY_DECLARATION_H_
