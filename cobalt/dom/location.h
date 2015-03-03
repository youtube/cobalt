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
//   http://www.w3.org/TR/html5/browsers.html#the-location-interface
class Location : public script::Wrappable {
 public:
  explicit Location(const GURL& url);

  // Web API: Location
  std::string href() const;
  void set_href(const std::string& search) { NOTIMPLEMENTED(); }

  std::string search() const;
  void set_search(const std::string& search) { NOTIMPLEMENTED(); }

 private:
  ~Location() OVERRIDE {}

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(Location);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_LOCATION_H_
