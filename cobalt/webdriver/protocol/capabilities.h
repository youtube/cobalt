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

#ifndef WEBDRIVER_PROTOCOL_CAPABILITIES_H_
#define WEBDRIVER_PROTOCOL_CAPABILITIES_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Documentation about capabilities can be found here:
// https://code.google.com/p/selenium/wiki/DesiredCapabilities
class Capabilities {
 public:
  // Convert a Capabilities object to/from a base::Value
  static scoped_ptr<base::Value> ToValue(const Capabilities& capabilities);
  static scoped_ptr<Capabilities> FromValue(const base::Value* value);

  // Create the actual capabilities of Cobalt's WebDriver implementation.
  static scoped_ptr<Capabilities> CreateActualCapabilities();

  // Return true if we support all the capabilities listed here.
  bool AreCapabilitiesSupported() const;

 private:
  Capabilities() {}
  // The capabilities listed here:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Capabilities_JSON_Object

  base::optional<std::string> browser_name_;
  base::optional<std::string> version_;
  base::optional<std::string> platform_;
  base::optional<bool> javascript_enabled_;
  base::optional<bool> takes_screenshot_;
  base::optional<bool> handles_alerts_;
  base::optional<bool> database_enabled_;
  base::optional<bool> location_context_enabled_;
  base::optional<bool> application_cache_enabled_;
  base::optional<bool> browser_connection_enabled_;
  base::optional<bool> css_selectors_enabled_;
  base::optional<bool> web_storage_enabled_;
  base::optional<bool> rotatable_;
  base::optional<bool> accept_ssl_certs_;
  base::optional<bool> native_events_;
  // TODO(***REMOVED***): Read proxy capability.

  // The client may request other capabilities that we don't know about. Keep
  // track of them here.
  scoped_ptr<base::DictionaryValue> other_capabilities_;
};

// Clients can provide two sets of Capabilities objects - one specifying
// capabilities they would like to have, and another specifying capabilities
// that the session must have.
class RequestedCapabilities {
 public:
  static scoped_ptr<RequestedCapabilities> FromValue(const base::Value* value);

  const Capabilities* desired() { return desired_.get(); }
  const Capabilities* required() { return required_.get(); }

 private:
  RequestedCapabilities() {}
  scoped_ptr<Capabilities> desired_;
  scoped_ptr<Capabilities> required_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // WEBDRIVER_PROTOCOL_CAPABILITIES_H_
