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

#ifndef STARBOARD_COMMON_RESET_AND_RETURN_H_
#define STARBOARD_COMMON_RESET_AND_RETURN_H_

namespace starboard {
namespace common {

template <typename T>
T ResetAndReturn(T* t) {
  T result(*t);
  *t = T();
  return result;
}

}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_RESET_AND_RETURN_H_
