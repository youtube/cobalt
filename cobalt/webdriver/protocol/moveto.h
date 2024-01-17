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

#include "base/values.h"
#include "cobalt/webdriver/protocol/element_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Represents the JSON parameters passed to the moveto WebDriver command.
// https://www.selenium.dev/documentation/legacy/json_wire_protocol/#sessionsessionidmoveto
class Moveto {
 public:
  static std::unique_ptr<base::Value> ToValue(const Moveto& moveto);
  static absl::optional<Moveto> FromValue(const base::Value* moveto);

  const absl::optional<ElementId>& element() const { return element_; }
  const absl::optional<int>& xoffset() const { return xoffset_; }
  const absl::optional<int>& yoffset() const { return yoffset_; }

 private:
  Moveto(const absl::optional<ElementId>& element,
         const absl::optional<int>& xoffset, const absl::optional<int>& yoffset)
      : element_(element), xoffset_(xoffset), yoffset_(yoffset) {}
  const absl::optional<ElementId> element_;
  const absl::optional<int> xoffset_;
  const absl::optional<int> yoffset_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_MOVETO_H_
