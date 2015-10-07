/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_CSS_DECLARATION_DATA_H_
#define CSSOM_CSS_DECLARATION_DATA_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// CSSDeclarationData which has PropertyValue type properties only used
// internally and it is not exposed to JavaScript.
class CSSDeclarationData : public base::RefCounted<CSSDeclarationData> {
 public:
  scoped_refptr<PropertyValue> GetPropertyValue(
      const std::string& property_name);
  void SetPropertyValue(const std::string& property_name,
                        const scoped_refptr<PropertyValue>& property_value);

  // Sets and gets property's importance. Default behaviour is not to record
  // importance.
  // Note that the const char* name passed in here should point to one of the
  // string constants in property_names.h, rather than pointing to a copy of the
  // string somewhere else, so they they can be property indexed and looked up.
  virtual bool IsPropertyImportant(const char* property_name) const {
    UNREFERENCED_PARAMETER(property_name);
    return false;
  }
  virtual void SetPropertyImportant(const char* property_name) {
    UNREFERENCED_PARAMETER(property_name);
  }

 protected:
  virtual ~CSSDeclarationData() {}

 private:
  virtual scoped_refptr<PropertyValue>* GetPropertyValueReference(
      const std::string& property_name) = 0;

  friend class base::RefCounted<CSSDeclarationData>;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_DECLARATION_DATA_H_
