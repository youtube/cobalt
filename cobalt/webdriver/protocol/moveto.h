// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_PROTOCOL_MOVETO_H_
#define COBALT_WEBDRIVER_PROTOCOL_MOVETO_H_

#include <memory>

#include "base/optional.h"
#include "base/values.h"
#include "cobalt/webdriver/protocol/element_id.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Represents the JSON parameters passed to the moveto WebDriver command.
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#sessionsessionidmoveto
class Moveto {
 public:
  static std::unique_ptr<base::Value> ToValue(const Moveto& moveto);
  static base::Optional<Moveto> FromValue(const base::Value* moveto);

  const base::Optional<ElementId>& element() const { return element_; }
  const base::Optional<int>& xoffset() const { return xoffset_; }
  const base::Optional<int>& yoffset() const { return yoffset_; }

 private:
  Moveto(const base::Optional<ElementId>& element,
         const base::Optional<int>& xoffset, const base::Optional<int>& yoffset)
      : element_(element), xoffset_(xoffset), yoffset_(yoffset) {}
  const base::Optional<ElementId> element_;
  const base::Optional<int> xoffset_;
  const base::Optional<int> yoffset_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_MOVETO_H_
