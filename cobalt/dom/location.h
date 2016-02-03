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

#ifndef COBALT_DOM_LOCATION_H_
#define COBALT_DOM_LOCATION_H_

#include <string>

#include "base/callback.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/script/wrappable.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// Each Document object in a browsing context's session history is associated
// with a unique instance of a Location object.
//   https://www.w3.org/TR/html5/browsers.html#the-location-interface
// Note(***REMOVED***): The Location object itself will not change its URL. The
// navigation callback should call set_url() or create new Location object.
class Location : public script::Wrappable {
 public:
  Location(const GURL& url,
           const base::Callback<void(const GURL&)>& navigation_callback,
           const csp::SecurityCallback& security_callback);

  // Web API: Location
  //
  // NOTE(***REMOVED***): Assign is implemented using Replace, which means navigation
  // history is not preserved.
  void Assign(const std::string& url) { Replace(url); }

  void Replace(const std::string& url);

  void Reload();

  // Web API: URLUtils (implements)
  //
  std::string href() const;
  void set_href(const std::string& href);

  std::string protocol() const;
  void set_protocol(const std::string& protocol);

  std::string host() const;
  void set_host(const std::string& host);

  std::string hostname() const;
  void set_hostname(const std::string& hostname);

  std::string port() const;
  void set_port(const std::string& port);

  std::string pathname() const;
  void set_pathname(const std::string& pathname);

  std::string hash() const;
  void set_hash(const std::string& hash);

  std::string search() const;
  void set_search(const std::string& search);

  // Custom, not in any spec.
  //
  const base::Callback<void(const GURL&)>& navigation_callback() const {
    return navigation_callback_;
  }

  // Gets and sets the URL without doing any navigation.
  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  DEFINE_WRAPPABLE_TYPE(Location);

 private:
  ~Location() OVERRIDE {}

  void Navigate(const GURL& url);

  GURL url_;
  base::Callback<void(const GURL&)> navigation_callback_;
  csp::SecurityCallback security_callback_;

  DISALLOW_COPY_AND_ASSIGN(Location);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOCATION_H_
