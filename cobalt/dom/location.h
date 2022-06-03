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

#ifndef COBALT_DOM_LOCATION_H_
#define COBALT_DOM_LOCATION_H_

#include <string>

#include "base/callback.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/navigation_type.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/location_base.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

// Location objects provide a representation of the address of the active
// document of their Document's browsing context, and allow the current entry of
// the browsing context's session history to be changed, by adding or replacing
// entries in the history object.
//   https://www.w3.org/TR/html50/browsers.html#the-location-interface
class Location : public web::LocationBase {
 public:
  // If any navigation is triggered, all these callbacks should be provided,
  // otherwise they can be empty.
  explicit Location(const GURL& url) : web::LocationBase(url) {}
  Location(const GURL& url, const base::Closure& hashchange_callback,
           const base::Callback<void(const GURL&)>& navigation_callback,
           const csp::SecurityCallback& security_callback,
           const base::Callback<void(NavigationType type)>&
               set_navigation_type_callback);
  Location(const Location&) = delete;
  Location& operator=(const Location&) = delete;

  // Web API: Location
  //
  // NOTE: Assign is implemented using Replace, which means navigation
  // history is not preserved.
  void Assign(const std::string& url) { Replace(url); }

  void Replace(const std::string& url);

  void Reload();

  DEFINE_WRAPPABLE_TYPE(Location);

 private:
  ~Location() override {}

  base::Closure hashchange_callback_;
  base::Callback<void(const GURL&)> navigation_callback_;
  csp::SecurityCallback security_callback_;
  const base::Callback<void(NavigationType)> set_navigation_type_callback_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOCATION_H_
