// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_TVOS_BUNDLE_DATA_H_
#define COBALT_BROWSER_TVOS_BUNDLE_DATA_H_

#include <optional>
#include <string>
#include <string_view>

namespace cobalt {

class BundleData final {
 public:
  // Retrieves a given value from the main bundle's Info.plist and returns it as
  // an std::string if it exists and can be converted to an NSString*. Returns
  // std::nullopt otherwise.
  static std::optional<std::string> GetValueFromPlistAsString(
      std::string_view key_name);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_TVOS_BUNDLE_DATA_H_
