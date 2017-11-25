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

#ifndef COBALT_DOM_URL_UTILS_H_
#define COBALT_DOM_URL_UTILS_H_

#include <string>

#include "base/callback.h"
#include "cobalt/loader/origin.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// Utility methods and properties that help work with URL.
//   https://www.w3.org/TR/2014/WD-url-1-20141209/#dom-urlutils
// NOTE: This interface has NoInterfaceObject prefix in IDL, therefore
// it is not a wrappable. Other interfaces that implement it can own or inherit
// from it to use it as a utility.
// It is possible to make the URL invalid by using the setters. The user should
// check url().is_valid().
// If a custom update steps callback is provided, this URLUtils object will not
// update the url_ field on any setters, in case the object is destroyed in the
// callback. The callback should call set_url if necessary.
class URLUtils {
 public:
  typedef base::Callback<void(const std::string&)> UpdateStepsCallback;

  explicit URLUtils(const GURL& url);
  explicit URLUtils(const UpdateStepsCallback& update_steps)
      : update_steps_(update_steps) {}
  URLUtils(const GURL& url, const UpdateStepsCallback& update_steps);

  // From the spec: URLUtils.
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

  std::string origin() const;

  // Custom, not in any spec.
  //
  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  loader::Origin GetOriginAsObject() const;

 private:
  // From the spec: URLUtils.
  void RunPreUpdateSteps(const GURL& new_url, const std::string& value);

  GURL url_;
  UpdateStepsCallback update_steps_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_URL_UTILS_H_
