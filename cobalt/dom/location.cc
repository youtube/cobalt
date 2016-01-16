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

#include "cobalt/dom/location.h"

#include "base/logging.h"

namespace cobalt {
namespace dom {

Location::Location(const GURL& url) : url_(url) {}

Location::Location(const GURL& url,
                   const base::Callback<void(const GURL&)>& navigation_callback)
    : url_(url), navigation_callback_(navigation_callback) {}

void Location::Replace(const std::string& url) {
  // When the replace(url) method is invoked, the UA must resolve the argument,
  // relative to the API base URL specified by the entry settings object, and if
  // that is successful, navigate the browsing context to the specified url with
  // replacement enabled and exceptions enabled.
  GURL new_url = url_.Resolve(url);
  Navigate(new_url);
}

void Location::Reload() {
  if (!navigation_callback_.is_null()) {
    navigation_callback_.Run(url_);
  }
}

std::string Location::href() const { return url_.spec(); }

void Location::set_href(const std::string& href) { Replace(href); }

std::string Location::protocol() const {
  return url_.is_empty() ? ":" : url_.scheme() + ":";
}

void Location::set_protocol(const std::string& protocol) {
  url_parse::Component component;
  component.len = static_cast<int>(protocol.length());
  GURL::Replacements replacements;
  replacements.SetScheme(protocol.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::host() const {
  if (url_.is_empty()) {
    return "";
  }
  return url_.host() + (url_.has_port() ? ":" + url_.port() : "");
}

void Location::set_host(const std::string& host_and_port) {
  // Host may include an optional port.
  std::string host = host_and_port;
  std::string port;
  size_t host_end = host.find(':');
  if (host_end != std::string::npos) {
    host = host_and_port.substr(0, host_end);
    port = host_and_port.substr(host_end + 1);
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  if (port.length() > 0) {
    replacements.SetPortStr(port);
  } else {
    replacements.ClearPort();
  }
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::hostname() const {
  return url_.is_empty() ? "" : url_.host();
}

void Location::set_hostname(const std::string& hostname) {
  url_parse::Component component;
  component.len = static_cast<int>(hostname.length());
  GURL::Replacements replacements;
  replacements.SetHost(hostname.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::port() const {
  return url_.is_empty() ? "" : url_.port();
}

void Location::set_port(const std::string& port) {
  url_parse::Component component;
  component.len = static_cast<int>(port.length());
  GURL::Replacements replacements;
  replacements.SetPort(port.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::pathname() const {
  return url_.is_empty() ? "" : url_.path();
}

void Location::set_pathname(const std::string& pathname) {
  url_parse::Component component;
  component.len = static_cast<int>(pathname.length());
  GURL::Replacements replacements;
  replacements.SetPath(pathname.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::hash() const {
  return url_.ref().empty() ? "" : "#" + url_.ref();
}

void Location::set_hash(const std::string& hash) {
  url_parse::Component component;
  component.len = static_cast<int>(hash.length());
  GURL::Replacements replacements;
  replacements.SetRef(hash.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

std::string Location::search() const {
  return url_.query().empty() ? "" : "?" + url_.query();
}

void Location::set_search(const std::string& search) {
  url_parse::Component component;
  component.len = static_cast<int>(search.length());
  GURL::Replacements replacements;
  replacements.SetQuery(search.c_str(), component);
  GURL new_url = url_.ReplaceComponents(replacements);
  Navigate(new_url);
}

// Algorithm for Navigate:
//   http://www.w3.org/TR/html5/browsers.html#navigate
void Location::Navigate(const GURL& url) {
  // Custom, not in any spec.
  if (!url.is_valid()) {
    DLOG(INFO) << "URL " << url << " is not valid, aborting the navigation.";
    return;
  }

  DLOG(INFO) << "Navigating to " << url;

  // 7. Fragment identifiers: Apply the URL parser algorithm to the absolute URL
  // of the new resource and the address of the active document of the browsing
  // context being navigated. If all the components of the resulting parsed
  // URLs, ignoring any fragment components, are identical, and the new resource
  // is to be fetched using HTTP GET or equivalent, and the parsed URL of the
  // new resource has a fragment component that is not null (even if it is
  // empty), then navigate to that fragment identifier and abort these steps.
  // NOTE(***REMOVED***): This means, if the new url doesn't have hash, we should
  // always navigate to the new url, even if it is the same as the current one.
  url_parse::Component component;
  component.len = 0;
  GURL::Replacements replacements;
  replacements.SetRef("", component);
  if (url.ReplaceComponents(replacements) ==
          url_.ReplaceComponents(replacements) &&
      url.has_ref()) {
    url_ = url;
    // TODO(***REMOVED***): Fire "hashchange" event at the window object.
  } else {
    if (!navigation_callback_.is_null()) {
      navigation_callback_.Run(url);
    }
  }
}

}  // namespace dom
}  // namespace cobalt
