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

#ifndef DOM_LOCATION_H_
#define DOM_LOCATION_H_

#include <string>

#include "base/logging.h"
#include "cobalt/script/wrappable.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// Each Document object in a browsing context's session history is associated
// with a unique instance of a Location object.
// Currently we only allow the change of the URL's hash, through the href setter
// or hash setter.
//   http://www.w3.org/TR/html5/browsers.html#the-location-interface
class Location : public script::Wrappable {
 public:
  explicit Location(const GURL& url);

  // Web API: Location
  //
  void Assign(const std::string& url);

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

  DEFINE_WRAPPABLE_TYPE(Location);

 private:
  ~Location() OVERRIDE {}

  void AssignInternal(const GURL& url);

  GURL url_;
  url_parse::Component empty_component_;
  GURL::Replacements remove_ref_;

  DISALLOW_COPY_AND_ASSIGN(Location);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_LOCATION_H_
