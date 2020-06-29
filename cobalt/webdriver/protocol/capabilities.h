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

#include "base/optional.h"
#include "base/values.h"
#include "cobalt/webdriver/protocol/proxy.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Documentation about capabilities can be found here:
// https://code.google.com/p/selenium/wiki/DesiredCapabilities
class Capabilities {
 public:
  // Convert a Capabilities object to/from a base::Value
  static std::unique_ptr<base::Value> ToValue(const Capabilities& capabilities);
  static base::Optional<Capabilities> FromValue(const base::Value* value);

  // Create the actual capabilities of Cobalt's WebDriver implementation.
  static Capabilities CreateActualCapabilities();

  // Return true if we support all the capabilities listed here.
  bool AreCapabilitiesSupported() const;

  base::Optional<Proxy> proxy() const { return proxy_; }

 private:
  Capabilities() {}
  // The capabilities listed here:
  // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Capabilities-JSON-Object

  base::Optional<std::string> browser_name_;
  base::Optional<std::string> version_;
  base::Optional<std::string> platform_;
  base::Optional<bool> javascript_enabled_;
  base::Optional<bool> takes_screenshot_;
  base::Optional<bool> handles_alerts_;
  base::Optional<bool> database_enabled_;
  base::Optional<bool> location_context_enabled_;
  base::Optional<bool> application_cache_enabled_;
  base::Optional<bool> browser_connection_enabled_;
  base::Optional<bool> css_selectors_enabled_;
  base::Optional<bool> web_storage_enabled_;
  base::Optional<bool> rotatable_;
  base::Optional<bool> accept_ssl_certs_;
  base::Optional<bool> native_events_;
  base::Optional<Proxy> proxy_;
};

// Clients can provide two sets of Capabilities objects - one specifying
// capabilities they would like to have, and another specifying capabilities
// that the session must have.
class RequestedCapabilities {
 public:
  static base::Optional<RequestedCapabilities> FromValue(
      const base::Value* value);

  const Capabilities& desired() const { return desired_; }
  const base::Optional<Capabilities>& required() const { return required_; }

 private:
  explicit RequestedCapabilities(const Capabilities& desired)
      : desired_(desired) {}
  RequestedCapabilities(const Capabilities& desired,
                        const Capabilities& required)
      : desired_(desired), required_(required) {}
  Capabilities desired_;
  base::Optional<Capabilities> required_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_CAPABILITIES_H_
