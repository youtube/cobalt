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

#include "cobalt/csp/source.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "url/url_canon.h"

namespace cobalt {
namespace csp {

Source::Source(ContentSecurityPolicy* policy, const SourceConfig& config)
    : policy_(policy) {
  // All comparisons require case-insensitivity.
  // Lower-case everything here to simplify that.
  config_.scheme = base::ToLowerASCII(config.scheme);
  config_.host = base::ToLowerASCII(config.host);
  config_.path = base::ToLowerASCII(config.path);
  config_.port = config.port;
  config_.host_wildcard = config.host_wildcard;
  config_.port_wildcard = config.port_wildcard;
}

// https://www.w3.org/TR/2015/CR-CSP2-20150721/#match-source-expression
bool Source::Matches(
    const GURL& url,
    ContentSecurityPolicy::RedirectStatus redirect_status) const {
  if (!SchemeMatches(url)) {
    return false;
  }
  if (IsSchemeOnly()) {
    return true;
  }
  if (!HostMatches(url)) {
    return false;
  }
  if (!PortMatches(url)) {
    return false;
  }

  bool paths_match = (redirect_status == ContentSecurityPolicy::kDidRedirect) ||
                     PathMatches(url);
  return paths_match;
}

bool Source::SchemeMatches(const GURL& url) const {
  if (config_.scheme.empty()) {
    return policy_->SchemeMatchesSelf(url);
  }
  if (base::LowerCaseEqualsASCII(config_.scheme, "http")) {
    return url.SchemeIs("http") || url.SchemeIs("https");
  }
  if (base::LowerCaseEqualsASCII(config_.scheme, "ws")) {
    return url.SchemeIs("ws") || url.SchemeIs("wss");
  }
  return url.SchemeIs(config_.scheme.c_str());
}

bool Source::HostMatches(const GURL& url) const {
  const std::string& host = url.host();
  bool match;
  if (config_.host_wildcard == SourceConfig::kHasWildcard) {
    match =
        base::EndsWith(host, "." + config_.host, base::CompareCase::SENSITIVE);
  } else {
    match = base::LowerCaseEqualsASCII(host, config_.host.c_str());
  }
  return match;
}

bool Source::PathMatches(const GURL& url) const {
  if (config_.path.empty()) {
    return true;
  }

  std::string path = base::ToLowerASCII(url.path());
  if (base::EndsWith(config_.path, "/", base::CompareCase::SENSITIVE)) {
    return StartsWith(path, config_.path, base::CompareCase::SENSITIVE);
  } else {
    return path == config_.path;
  }
}
// https://www.w3.org/TR/2015/CR-CSP2-20150721/#match-source-expression #9-10
bool Source::PortMatches(const GURL& url) const {
  if (config_.port_wildcard == SourceConfig::kHasWildcard) {
    return true;
  }

  // Matches if ports are the same. If a port is unspecified, consider it as
  // the default port for url's scheme.
  const std::string& url_scheme = url.scheme();
  int config_port = config_.port;
  if (config_port == url::PORT_UNSPECIFIED) {
    config_port = url::DefaultPortForScheme(
        url_scheme.c_str(), static_cast<int>(url_scheme.length()));
  }
  int url_port = url.EffectiveIntPort();
  if (url_port == config_port) {
    return true;
  }

  return false;
}

bool Source::IsSchemeOnly() const { return config_.host.empty(); }

}  // namespace csp
}  // namespace cobalt
