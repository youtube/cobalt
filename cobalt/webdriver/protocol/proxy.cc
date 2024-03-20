// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/proxy.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {

const char kProxyTypeKey[] = "proxyType";

// Used for manual proxy type. Webdriver also supports ftp proxy and socks
// proxy, but these are ignored.
const char kHttpProxyKey[] = "httpProxy";
const char kSslProxyKey[] = "sslProxy";

// Supported values for proxyType.
const char kManualProxyType[] = "manual";
}  // namespace

base::Optional<Proxy> Proxy::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return base::nullopt;
  }
  const std::string* proxy_type_string_ptr =
      dictionary_value->FindString(kProxyTypeKey);
  if (!proxy_type_string_ptr) {
    return base::nullopt;
  }
  const std::string proxy_type_string =
      base::ToLowerASCII(*proxy_type_string_ptr);
  if (proxy_type_string == kManualProxyType) {
    std::vector<std::string> proxy_rules;
    const std::string* http_proxy = dictionary_value->FindString(kHttpProxyKey);
    if (http_proxy) {
      proxy_rules.push_back(std::string("http=") + *http_proxy);
    }
    const std::string* https_proxy = dictionary_value->FindString(kSslProxyKey);
    if (https_proxy) {
      proxy_rules.push_back(std::string("https=") + *https_proxy);
    }
    if (!proxy_rules.empty()) {
      return Proxy(base::JoinString(proxy_rules, ";"));
    }
  } else {
    // Only manual proxy type is supported.
    DLOG(INFO) << "Unsupported proxy type: " << proxy_type_string;
  }
  return base::nullopt;
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
