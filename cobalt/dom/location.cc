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

namespace cobalt {
namespace dom {

Location::Location(const GURL& url) : url_(url) {
  empty_component_.len = 0;
  remove_ref_.SetRef("", empty_component_);
}

void Location::Assign(const std::string& url) { AssignInternal(GURL(url)); }

void Location::Reload() {}

std::string Location::href() const { return url_.spec(); }

void Location::set_href(const std::string& href) { Assign(href); }

std::string Location::protocol() const {
  return url_.is_empty() ? ":" : url_.scheme() + ":";
}

void Location::set_protocol(const std::string& protocol) {
  UNREFERENCED_PARAMETER(protocol);
  NOTREACHED();
}

std::string Location::host() const {
  if (url_.is_empty()) {
    return "";
  }
  return url_.host() + (url_.has_port() ? ":" + url_.port() : "");
}

void Location::set_host(const std::string& host) {
  UNREFERENCED_PARAMETER(host);
  NOTREACHED();
}

std::string Location::hostname() const {
  return url_.is_empty() ? "" : url_.host();
}

void Location::set_hostname(const std::string& hostname) {
  UNREFERENCED_PARAMETER(hostname);
  NOTREACHED();
}

std::string Location::port() const {
  return url_.is_empty() ? "" : url_.port();
}

void Location::set_port(const std::string& port) {
  UNREFERENCED_PARAMETER(port);
  NOTREACHED();
}

std::string Location::hash() const {
  return url_.ref().empty() ? "" : "#" + url_.ref();
}

void Location::set_hash(const std::string& hash) {
  url_parse::Component comp;
  comp.len = static_cast<int>(hash.length());
  GURL::Replacements repl;
  repl.SetRef(hash.c_str(), comp);
  GURL new_url = url_.ReplaceComponents(repl);
  AssignInternal(new_url);
}

std::string Location::search() const {
  return url_.query().empty() ? "" : "?" + url_.query();
}

void Location::set_search(const std::string& search) {
  UNREFERENCED_PARAMETER(search);
  NOTREACHED();
}

void Location::AssignInternal(const GURL& url) {
  if (url.ReplaceComponents(remove_ref_) ==
      url_.ReplaceComponents(remove_ref_)) {
    url_ = url;
    // TODO(***REMOVED***): Fire "hashchange" event at the window object.
  } else {
    LOG(WARNING) << "Only the hash of the URL in Location can be changed. The "
                    "following change is ignored: from " << url_ << " to "
                 << url << ".";
  }
}

}  // namespace dom
}  // namespace cobalt
