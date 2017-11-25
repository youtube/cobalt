// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/url_utils.h"

namespace cobalt {
namespace dom {

URLUtils::URLUtils(const GURL& url) : url_(url) {}
URLUtils::URLUtils(const GURL& url, const UpdateStepsCallback& update_steps)
    : url_(url), update_steps_(update_steps) {}

std::string URLUtils::href() const { return url_.possibly_invalid_spec(); }

void URLUtils::set_href(const std::string& href) {
  GURL new_url = GURL(href);
  RunPreUpdateSteps(new_url, href);
}

std::string URLUtils::protocol() const { return url_.scheme() + ":"; }

void URLUtils::set_protocol(const std::string& protocol) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(protocol);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::host() const {
  return url_.host() + (url_.has_port() ? ":" + url_.port() : "");
}

void URLUtils::set_host(const std::string& host) {
  // Host may include an optional port.
  std::string host_without_port = host;
  std::string port;
  size_t colon_pos = host.find(':');
  if (colon_pos != std::string::npos) {
    host_without_port = host.substr(0, colon_pos);
    port = host.substr(colon_pos + 1);
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(host_without_port);
  if (port.length() > 0) {
    replacements.SetPortStr(port);
  }
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::hostname() const { return url_.host(); }

void URLUtils::set_hostname(const std::string& hostname) {
  GURL::Replacements replacements;
  replacements.SetHostStr(hostname);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::port() const { return url_.port(); }

void URLUtils::set_port(const std::string& port) {
  GURL::Replacements replacements;
  replacements.SetPortStr(port);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::pathname() const { return url_.path(); }

void URLUtils::set_pathname(const std::string& pathname) {
  GURL::Replacements replacements;
  replacements.SetPathStr(pathname);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::hash() const {
  return url_.has_ref() ? "#" + url_.ref() : "";
}

// Algorithm for set_hash:
//   https://www.w3.org/TR/2014/WD-url-1-20141209/#dom-urlutils-hash
void URLUtils::set_hash(const std::string& hash) {
  // 3. Let input be the given value with a single leading "#" removed, if any.
  std::string hash_value =
      !hash.empty() && hash[0] == '#' ? hash.substr(1) : hash;

  GURL::Replacements replacements;
  replacements.SetRefStr(hash_value);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

std::string URLUtils::search() const {
  return url_.has_query() ? "?" + url_.query() : "";
}

// Algorithm for set_search:
//   https://www.w3.org/TR/2014/WD-url-1-20141209/#dom-urlutils-search
void URLUtils::set_search(const std::string& search) {
  // 3. Let input be the given value with a single leading "?" removed, if any.
  std::string search_value =
      !search.empty() && search[0] == '?' ? search.substr(1) : search;

  GURL::Replacements replacements;
  replacements.SetQueryStr(search_value);
  GURL new_url = url_.ReplaceComponents(replacements);
  RunPreUpdateSteps(new_url, "");
}

// Algorithm for RunPreUpdateSteps:
//   https://www.w3.org/TR/2014/WD-url-1-20141209/#pre-update-steps
void URLUtils::RunPreUpdateSteps(const GURL& new_url,
                                 const std::string& value) {
  DLOG(INFO) << "Update URL to " << new_url.possibly_invalid_spec();

  // 1. If value is not given, let value be the result of serializing the
  // associated url.
  // 2, Run the update steps with value.
  if (update_steps_.is_null()) {
    url_ = new_url;
  } else {
    update_steps_.Run(value.empty() ? new_url.possibly_invalid_spec() : value);
  }
}

std::string URLUtils::origin() const {
  return GetOriginAsObject().SerializedOrigin();
}

loader::Origin URLUtils::GetOriginAsObject() const {
  return loader::Origin(url_);
}

}  // namespace dom
}  // namespace cobalt
