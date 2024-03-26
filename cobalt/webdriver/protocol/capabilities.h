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

#ifndef COBALT_WEBDRIVER_PROTOCOL_CAPABILITIES_H_
#define COBALT_WEBDRIVER_PROTOCOL_CAPABILITIES_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "cobalt/webdriver/protocol/proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Documentation about capabilities can be found here:
// https://code.google.com/p/selenium/wiki/DesiredCapabilities
class Capabilities {
 public:
  // Convert a Capabilities object to/from a base::Value
  static std::unique_ptr<base::Value> ToValue(const Capabilities& capabilities);
  static absl::optional<Capabilities> FromValue(const base::Value* value);

  // Create the actual capabilities of Cobalt's WebDriver implementation.
  static Capabilities CreateActualCapabilities();

  // Return true if we support all the capabilities listed here.
  bool AreCapabilitiesSupported() const;

  absl::optional<Proxy> proxy() const { return proxy_; }

 private:
  Capabilities() {}
  // The capabilities listed here:
  // https://www.selenium.dev/documentation/legacy/json_wire_protocol/#capabilities-json-object

  absl::optional<std::string> browser_name_;
  absl::optional<std::string> version_;
  absl::optional<std::string> platform_;
  absl::optional<bool> javascript_enabled_;
  absl::optional<bool> takes_screenshot_;
  absl::optional<bool> handles_alerts_;
  absl::optional<bool> database_enabled_;
  absl::optional<bool> location_context_enabled_;
  absl::optional<bool> application_cache_enabled_;
  absl::optional<bool> browser_connection_enabled_;
  absl::optional<bool> css_selectors_enabled_;
  absl::optional<bool> web_storage_enabled_;
  absl::optional<bool> rotatable_;
  absl::optional<bool> accept_ssl_certs_;
  absl::optional<bool> native_events_;
  absl::optional<Proxy> proxy_;
};

// Clients can provide two sets of Capabilities objects - one specifying
// capabilities they would like to have, and another specifying capabilities
// that the session must have.
class RequestedCapabilities {
 public:
  static absl::optional<RequestedCapabilities> FromValue(
      const base::Value* value);

  const Capabilities& desired() const { return desired_; }
  const absl::optional<Capabilities>& required() const { return required_; }

 private:
  explicit RequestedCapabilities(const Capabilities& desired)
      : desired_(desired) {}
  RequestedCapabilities(const Capabilities& desired,
                        const Capabilities& required)
      : desired_(desired), required_(required) {}
  Capabilities desired_;
  absl::optional<Capabilities> required_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_CAPABILITIES_H_
