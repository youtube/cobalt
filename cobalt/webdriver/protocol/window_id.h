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

#ifndef COBALT_WEBDRIVER_PROTOCOL_WINDOW_ID_H_
#define COBALT_WEBDRIVER_PROTOCOL_WINDOW_ID_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Opaque type that uniquely identifies a window from a WebDriver session.
class WindowId {
 public:
  static base::Optional<WindowId> FromValue(const base::Value* value);
  static std::unique_ptr<base::Value> ToValue(const WindowId& window_id) {
    return std::unique_ptr<base::Value>(new base::Value(window_id.id_));
  }

  // Requests can specify "current" as the window ID to get the current window
  // rather than specifying the id of the current window.
  static bool IsCurrent(const WindowId& window_id) {
    return window_id.id() == "current";
  }

  explicit WindowId(const std::string& id) : id_(id) {}
  const std::string& id() const { return id_; }
  bool operator==(const WindowId& rhs) const { return id_ == rhs.id_; }

 private:
  std::string id_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_WINDOW_ID_H_
