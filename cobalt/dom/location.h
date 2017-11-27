// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_LOCATION_H_
#define COBALT_DOM_LOCATION_H_

#include <string>

#include "base/callback.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/script/wrappable.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// Location objects provide a representation of the address of the active
// document of their Document's browsing context, and allow the current entry of
// the browsing context's session history to be changed, by adding or replacing
// entries in the history object.
//   https://www.w3.org/TR/html5/browsers.html#the-location-interface
class Location : public script::Wrappable {
 public:
  // If any navigation is triggered, all these callbacks should be provided,
  // otherwise they can be empty.
  Location(const GURL& url, const base::Closure& hashchange_callback,
           const base::Callback<void(const GURL&)>& navigation_callback,
           const csp::SecurityCallback& security_callback);

  // Web API: Location
  //
  // NOTE: Assign is implemented using Replace, which means navigation
  // history is not preserved.
  void Assign(const std::string& url) { Replace(url); }

  void Replace(const std::string& url);

  void Reload();

  // Web API: URLUtils (implements)
  //
  std::string href() const { return url_utils_.href(); }
  void set_href(const std::string& href) { url_utils_.set_href(href); }

  std::string protocol() const { return url_utils_.protocol(); }
  void set_protocol(const std::string& protocol) {
    url_utils_.set_protocol(protocol);
  }

  std::string host() const { return url_utils_.host(); }
  void set_host(const std::string& host) { url_utils_.set_host(host); }

  std::string hostname() const { return url_utils_.hostname(); }
  void set_hostname(const std::string& hostname) {
    url_utils_.set_hostname(hostname);
  }

  std::string port() const { return url_utils_.port(); }
  void set_port(const std::string& port) { url_utils_.set_port(port); }

  std::string pathname() const { return url_utils_.pathname(); }
  void set_pathname(const std::string& pathname) {
    url_utils_.set_pathname(pathname);
  }

  std::string hash() const { return url_utils_.hash(); }
  void set_hash(const std::string& hash) { url_utils_.set_hash(hash); }

  std::string search() const { return url_utils_.search(); }
  void set_search(const std::string& search) { url_utils_.set_search(search); }

  std::string origin() const { return url_utils_.origin(); }

  // Custom, not in any spec.
  //
  // Gets and sets the URL without doing any navigation.
  const GURL& url() const { return url_utils_.url(); }
  void set_url(const GURL& url) { url_utils_.set_url(url); }

  const loader::Origin& OriginObject() const {
    return url_utils_.OriginObject();
  }

  DEFINE_WRAPPABLE_TYPE(Location);

 private:
  ~Location() override {}

  URLUtils url_utils_;
  base::Closure hashchange_callback_;
  base::Callback<void(const GURL&)> navigation_callback_;
  csp::SecurityCallback security_callback_;

  DISALLOW_COPY_AND_ASSIGN(Location);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOCATION_H_
