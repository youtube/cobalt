// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_ORIGIN_H_
#define COBALT_LOADER_ORIGIN_H_

#include <string>

#include "googleurl/src/gurl.h"

namespace cobalt {
namespace loader {
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin
class Origin {
 public:
  // To create an opaque origin, use Origin().
  Origin();
  // Initialize an origin to the url's origin.
  // https://url.spec.whatwg.org/#concept-url-origin
  explicit Origin(const GURL& url);
  // https://html.spec.whatwg.org/multipage/origin.html#ascii-serialisation-of-an-origin
  std::string SerializedOrigin() const;
  bool is_opaque() const { return is_opaque_; }
  // Only document has an origin and no elements inherit document's origin, so
  // opaque origin comparison can always return false.
  // https://html.spec.whatwg.org/multipage/origin.html#same-origin
  bool operator==(const Origin& rhs) const;
  // Returns true if two origins are different(cross-origin).
  bool operator!=(const Origin& rhs) const { return !(*this == rhs); }

 private:
  bool is_opaque_;
  std::string origin_str_;
};
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_ORIGIN_H_
