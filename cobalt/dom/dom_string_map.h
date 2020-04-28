// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_DOM_STRING_MAP_H_
#define COBALT_DOM_DOM_STRING_MAP_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/property_enumerator.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Element;

// The DOMStringMap interface represents a set of name-value pairs. It exposes
// these using the scripting language's native mechanisms for property access.
//   https://www.w3.org/TR/html50/infrastructure.html#domstringmap
//
// NOTE: This implementation is targeted to support |HTMLElement.dataset|
//       which performs bidirectional conversion of key names between hyphenated
//       and camel-cased forms as per
//       https://www.w3.org/TR/html50/dom.html#dom-dataset.
class DOMStringMap : public script::Wrappable,
                     public base::SupportsWeakPtr<DOMStringMap> {
 public:
  explicit DOMStringMap(const scoped_refptr<Element>& element);

  // Web API: DOMStringMap
  base::Optional<std::string> AnonymousNamedGetter(
      const std::string& key, script::ExceptionState* exception_state);
  void AnonymousNamedSetter(const std::string& key, const std::string& value,
                            script::ExceptionState* exception_state);

  // Custom, not in any spec.
  bool CanQueryNamedProperty(const std::string& key) const;

  // Custom, not in any spec.
  void EnumerateNamedProperties(script::PropertyEnumerator* enumerator);

  DEFINE_WRAPPABLE_TYPE(DOMStringMap);

 private:
  ~DOMStringMap();

  // The associated element.
  scoped_refptr<Element> element_;

  DISALLOW_COPY_AND_ASSIGN(DOMStringMap);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_STRING_MAP_H_
