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

#ifndef COBALT_WEBDRIVER_PROTOCOL_ELEMENT_ID_H_
#define COBALT_WEBDRIVER_PROTOCOL_ELEMENT_ID_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Opaque type that uniquely identifies an Element from a WebDriver session.
class ElementId {
 public:
  static const char kElementKey[];

  // Convert the ElementId to a WebElement JSON object:
  // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#WebElement-JSON-Object
  static std::unique_ptr<base::Value> ToValue(const ElementId& element_id);
  static base::Optional<ElementId> FromValue(const base::Value* value);

  explicit ElementId(const std::string& id) : id_(id) {}
  const std::string& id() const { return id_; }
  bool operator==(const ElementId& rhs) const { return id_ == rhs.id_; }

 private:
  std::string id_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_ELEMENT_ID_H_
