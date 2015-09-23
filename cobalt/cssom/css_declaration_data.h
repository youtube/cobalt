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
// internally and it is not exposed to JavaScript.  It derives from
// RefCountedThreadSafe instead of RefCounted so that it can be passed forward
// to another thread for animating before rendering a scene.
class CSSDeclarationData
    : public base::RefCountedThreadSafe<CSSDeclarationData> {
 public:
  virtual scoped_refptr<const PropertyValue> GetPropertyValue(
      const std::string& property_name) = 0;
  virtual void SetPropertyValueAndImportance(
      const std::string& property_name,
      const scoped_refptr<PropertyValue>& property_value, bool important) = 0;

 protected:
  virtual ~CSSDeclarationData() {}

 private:
  friend class base::RefCountedThreadSafe<CSSDeclarationData>;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_DECLARATION_DATA_H_
