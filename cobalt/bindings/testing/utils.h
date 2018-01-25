// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_UTILS_H_
#define COBALT_BINDINGS_TESTING_UTILS_H_

#include <string>

#include "base/stringprintf.h"

namespace cobalt {
namespace bindings {
namespace testing {

// In bindings tests that depend on a platform object prototype converted to a
// string, we accept both of these results because (e.g.)
// "String(Object.getPrototypeOf(document.body));" will evaluate to "[object
// HTMLBodyElement]" on Chrome 63 and "[object HTMLBodyElementPrototype]" on
// Firefox 57.  So, when we use any non-V8 engine, we handle this in the style
// of non-Chrome browsers, and when we use V8, we handle this in the style of
// Chrome.
inline bool IsAcceptablePrototypeString(const std::string& interface_name,
                                        const std::string& string) {
  auto spec_style_string =
      base::StringPrintf("[object %sPrototype]", interface_name.c_str());
  auto chrome_style_string =
      base::StringPrintf("[object %s]", interface_name.c_str());
  return string == spec_style_string || string == chrome_style_string;
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_UTILS_H_
