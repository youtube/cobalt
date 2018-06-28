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

#ifndef COBALT_WEBDRIVER_ELEMENT_MAPPING_H_
#define COBALT_WEBDRIVER_ELEMENT_MAPPING_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/element.h"
#include "cobalt/webdriver/protocol/element_id.h"

namespace cobalt {
namespace webdriver {

class ElementMapping {
 public:
  // Get (and possibly create) a mapping from this element to an ElementId.
  virtual protocol::ElementId ElementToId(
      const scoped_refptr<dom::Element>& element) = 0;
  // Get the element that this Id corresponds to, or NULL if no such element
  // exists.
  virtual scoped_refptr<dom::Element> IdToElement(
      const protocol::ElementId& id) = 0;
  virtual ~ElementMapping() {}
};

}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_ELEMENT_MAPPING_H_
