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

#ifndef COBALT_SCRIPT_MOZJS_45_TYPE_TRAITS_H_
#define COBALT_SCRIPT_MOZJS_45_TYPE_TRAITS_H_

namespace cobalt {
namespace script {
namespace mozjs {

template <typename T>
struct TypeTraits {
  // The type to convert into from a JS Value in the bindings implementation.
  typedef T ConversionType;
  // Type type returned from a Cobalt implementation of a bound function.
  typedef T ReturnType;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_TYPE_TRAITS_H_
