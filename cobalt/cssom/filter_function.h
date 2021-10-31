// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_FILTER_FUNCTION_H_
#define COBALT_CSSOM_FILTER_FUNCTION_H_

#include <string>

#include "cobalt/base/polymorphic_equatable.h"

namespace cobalt {
namespace cssom {

// A base class for filter functions. Filters manipulate an element's rendering.
//   https://www.w3.org/TR/filter-effects-1/
class FilterFunction : public base::PolymorphicEquatable {
 public:
  virtual std::string ToString() const = 0;

  virtual ~FilterFunction() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_FILTER_FUNCTION_H_
