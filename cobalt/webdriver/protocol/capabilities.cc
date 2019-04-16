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

#include <memory>
// limitations under the License.
#include "cobalt/webdriver/protocol/capabilities.h"

#include "base/values.h"
#include "cobalt/version.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {

const char kBrowserNameKey[] = "browserName";
const char kVersionKey[] = "version";
const char kPlatformKey[] = "platform";
const char kJavascriptEnabledKey[] = "javascriptEnabled";
const char kTakesScreenshotKey[] = "takesScreenshot";
const char kHandlesAlertsKey[] = "handlesAlerts";
const char kDatabaseEnabled[] = "databaseEnabled";
const char kLocationContextEnabledKey[] = "locationContextEnabled";
const char kApplicationCacheEnabledKey[] = "applicationCacheEnabled";
const char kBrowserConnectionEnabledKey[] = "browserConnectionEnabled";
const char kCssSelectorsEnabledKey[] = "cssSelectorsEnabled";
const char kWebStorageEnabledKey[] = "webStorageEnabled";
const char kRotatableKey[] = "rotatable";
const char kAcceptSslCertsKey[] = "acceptSslCerts";
const char kNativeEventsKey[] = "nativeEvents";
const char kProxyKey[] = "proxy";

const char kDesiredCapabilitiesKey[] = "desiredCapabilities";
const char kRequiredCapabilitiesKey[] = "requiredCapabilities";

// Helper class for reading from the base::DictionaryValue representing
// capabilities from the client into a Capabilities object.
// This will parse a capability and set its corresponding member in the
// Capabilities object, and then remove the handled capability from the
// source base::DictionaryValue.
class CapabilityReader {
 public:
  CapabilityReader(Capabilities* capabilities,
                   base::DictionaryValue* capabilities_value)
      : capabilities_(capabilities), capabilities_value_(capabilities_value) {}

  template <base::Optional<std::string> Capabilities::*member>
  void TryReadCapability(const char* key) {
    std::string value;
    if (capabilities_value_->GetString(key, &value)) {
      (capabilities_->*member) = value;
      bool removed = capabilities_value_->Remove(key, NULL);
      DCHECK(removed);
    }
  }

  template <base::Optional<bool> Capabilities::*member>
  void TryReadCapability(const char* key) {
    bool value;
    if (capabilities_value_->GetBoolean(key, &value)) {
      (capabilities_->*member) = value;
      bool removed = capabilities_value_->Remove(key, NULL);
      DCHECK(removed);
    }
  }

  template <typename T, base::Optional<T> Capabilities::*member>
  void TryReadCapability(const char* key) {
    const base::DictionaryValue* dictionary_value;
    if (capabilities_value_->GetDictionary(key, &dictionary_value)) {
      (capabilities_->*member) = T::FromValue(dictionary_value);
      if (capabilities_->*member) {
        bool removed = capabilities_value_->Remove(key, NULL);
        DCHECK(removed);
      }
    }
  }

 private:
  Capabilities* capabilities_;
  base::DictionaryValue* capabilities_value_;
};

class CapabilityWriter {
 public:
  CapabilityWriter(const Capabilities& capabilities,
                   base::DictionaryValue* capabilities_value)
      : capabilities_(capabilities), capabilities_value_(capabilities_value) {}

  template <base::Optional<std::string> Capabilities::*member>
  void TryWriteCapability(const char* key) {
    if (capabilities_.*member) {
      capabilities_value_->SetString(key, (capabilities_.*member).value_or(""));
    }
  }

  template <base::Optional<bool> Capabilities::*member>
  void TryWriteCapability(const char* key) {
    if (capabilities_.*member) {
      capabilities_value_->SetBoolean(key,
                                      (capabilities_.*member).value_or(false));
    }
  }

 private:
  const Capabilities& capabilities_;
  base::DictionaryValue* capabilities_value_;
};

}  // namespace

std::unique_ptr<base::Value> Capabilities::ToValue(
    const Capabilities& capabilities) {
  std::unique_ptr<base::DictionaryValue> capabilities_value(
      new base::DictionaryValue());

  CapabilityWriter writer(capabilities, capabilities_value.get());
  // Write all the capabilities that have been set.
  writer.TryWriteCapability<&Capabilities::browser_name_>(kBrowserNameKey);
  writer.TryWriteCapability<&Capabilities::version_>(kVersionKey);
  writer.TryWriteCapability<&Capabilities::platform_>(kPlatformKey);
  writer.TryWriteCapability<&Capabilities::javascript_enabled_>(
      kJavascriptEnabledKey);
  writer.TryWriteCapability<&Capabilities::takes_screenshot_>(
      kTakesScreenshotKey);
  writer.TryWriteCapability<&Capabilities::handles_alerts_>(kHandlesAlertsKey);
  writer.TryWriteCapability<&Capabilities::database_enabled_>(kDatabaseEnabled);
  writer.TryWriteCapability<&Capabilities::location_context_enabled_>(
      kLocationContextEnabledKey);
  writer.TryWriteCapability<&Capabilities::application_cache_enabled_>(
      kApplicationCacheEnabledKey);
  writer.TryWriteCapability<&Capabilities::browser_connection_enabled_>(
      kBrowserConnectionEnabledKey);
  writer.TryWriteCapability<&Capabilities::css_selectors_enabled_>(
      kCssSelectorsEnabledKey);
  writer.TryWriteCapability<&Capabilities::web_storage_enabled_>(
      kWebStorageEnabledKey);
  writer.TryWriteCapability<&Capabilities::rotatable_>(kRotatableKey);
  writer.TryWriteCapability<&Capabilities::accept_ssl_certs_>(
      kAcceptSslCertsKey);
  writer.TryWriteCapability<&Capabilities::native_events_>(kNativeEventsKey);

  return std::unique_ptr<base::Value>(capabilities_value.release());
}

base::Optional<Capabilities> Capabilities::FromValue(const base::Value* value) {
  const base::DictionaryValue* value_as_dictionary;
  if (!value->GetAsDictionary(&value_as_dictionary)) {
    return base::nullopt;
  }
  // Create a new Capabilities object, and copy the capabilities dictionary
  // from which we will read capabilities
  Capabilities capabilities;
  std::unique_ptr<base::DictionaryValue> capabilities_copy(
      value_as_dictionary->DeepCopy());

  // Read all the capabilities we know about, and remove handled capabilities
  // from the dictionary.
  CapabilityReader reader(&capabilities, capabilities_copy.get());
  reader.TryReadCapability<&Capabilities::browser_name_>(kBrowserNameKey);
  reader.TryReadCapability<&Capabilities::version_>(kVersionKey);
  reader.TryReadCapability<&Capabilities::platform_>(kPlatformKey);
  reader.TryReadCapability<&Capabilities::javascript_enabled_>(
      kJavascriptEnabledKey);
  reader.TryReadCapability<&Capabilities::takes_screenshot_>(
      kTakesScreenshotKey);
  reader.TryReadCapability<&Capabilities::handles_alerts_>(kHandlesAlertsKey);
  reader.TryReadCapability<&Capabilities::database_enabled_>(kDatabaseEnabled);
  reader.TryReadCapability<&Capabilities::location_context_enabled_>(
      kLocationContextEnabledKey);
  reader.TryReadCapability<&Capabilities::application_cache_enabled_>(
      kApplicationCacheEnabledKey);
  reader.TryReadCapability<&Capabilities::browser_connection_enabled_>(
      kBrowserConnectionEnabledKey);
  reader.TryReadCapability<&Capabilities::css_selectors_enabled_>(
      kCssSelectorsEnabledKey);
  reader.TryReadCapability<&Capabilities::web_storage_enabled_>(
      kWebStorageEnabledKey);
  reader.TryReadCapability<&Capabilities::rotatable_>(kRotatableKey);
  reader.TryReadCapability<&Capabilities::accept_ssl_certs_>(
      kAcceptSslCertsKey);
  reader.TryReadCapability<&Capabilities::native_events_>(kNativeEventsKey);

  reader.TryReadCapability<Proxy, &Capabilities::proxy_>(kProxyKey);
  return capabilities;
}

Capabilities Capabilities::CreateActualCapabilities() {
  Capabilities capabilities;
  // Set the capabilities that we know we support.
  capabilities.browser_name_ = "Cobalt";
  capabilities.version_ = COBALT_VERSION;
  // TODO: Support platforms other than Linux.
  capabilities.platform_ = "Linux";
  capabilities.javascript_enabled_ = true;
  capabilities.css_selectors_enabled_ = true;
  return capabilities;
}

bool Capabilities::AreCapabilitiesSupported() const {
  // TODO: Check for unsupported capabilities.
  return true;
}

base::Optional<RequestedCapabilities> RequestedCapabilities::FromValue(
    const base::Value* value) {
  DCHECK(value);
  const base::DictionaryValue* requested_capabilities_value;
  if (!value->GetAsDictionary(&requested_capabilities_value)) {
    return base::nullopt;
  }

  base::Optional<Capabilities> desired;
  const base::Value* capabilities_value = NULL;
  if (requested_capabilities_value->Get(kDesiredCapabilitiesKey,
                                        &capabilities_value)) {
    desired = Capabilities::FromValue(capabilities_value);
  }
  if (!desired) {
    // Desired capabilities are required.
    return base::nullopt;
  }
  base::Optional<Capabilities> required;
  if (requested_capabilities_value->Get(kRequiredCapabilitiesKey,
                                        &capabilities_value)) {
    required = Capabilities::FromValue(capabilities_value);
  }
  if (required) {
    return RequestedCapabilities(desired.value(), required.value());
  } else {
    return RequestedCapabilities(desired.value());
  }
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
