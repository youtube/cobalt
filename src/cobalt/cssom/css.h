// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_H_
#define COBALT_CSSOM_CSS_H_

#include <string>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

// The CSS interface allows querying for the support of CSS features from
// Javascript.
//   https://www.w3.org/TR/css3-conditional/#the-css-interface
class CSS : public script::Wrappable {
 public:
  static bool Supports(script::EnvironmentSettings* settings,
                       const std::string& property, const std::string& value);

  DEFINE_WRAPPABLE_TYPE(CSS);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_H_
