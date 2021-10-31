/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MATH_CLAMP_H_
#define COBALT_MATH_CLAMP_H_

#include <algorithm>

namespace cobalt {
namespace math {

template <typename T>
T Clamp(const T& input, const T& minimum, const T& maximum) {
  return std::max<T>(minimum, std::min<T>(maximum, input));
}

}  // namespace math.
}  // namespace cobalt.

#endif  // COBALT_MATH_CLAMP_H_
