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

#ifndef CSS_PARSER_PROPERTY_DECLARATION_H_
#define CSS_PARSER_PROPERTY_DECLARATION_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/css_parser/trivial_string_piece.h"
#include "cobalt/cssom/css_style_declaration_data.h"
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
  PropertyDeclaration(const std::string& name,
                      const scoped_refptr<cssom::PropertyValue>& value,
                      bool important)
      : important(important) {
    property_values.push_back(NameValuePair(name, value));
  }

  // Shorthand properties will construct an empty PropertyDeclaration object
  // using this constructor object and then populate it manually.
  explicit PropertyDeclaration(bool important) : important(important) {}

  void Apply(cssom::CSSDeclarationData* declaration_data) const {
    for (NameValuePairList::const_iterator iter = property_values.begin();
         iter != property_values.end(); ++iter) {
      declaration_data->SetPropertyValue(iter->name, iter->value);
      // TODO(***REMOVED***): Set property importance.
      DCHECK_EQ(iter->value, declaration_data->GetPropertyValue(iter->name));
    }
  }

  struct NameValuePair {
    NameValuePair(const std::string& name,
                  const scoped_refptr<cssom::PropertyValue>& value)
        : name(name), value(value) {}

    std::string name;
    scoped_refptr<cssom::PropertyValue> value;
  };
  typedef std::vector<NameValuePair> NameValuePairList;
  NameValuePairList property_values;
  bool important;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // CSS_PARSER_PROPERTY_DECLARATION_H_
