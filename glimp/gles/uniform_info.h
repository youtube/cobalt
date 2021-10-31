/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_UNIFORM_INFO_H_
#define GLIMP_GLES_UNIFORM_INFO_H_

namespace glimp {
namespace gles {

// Maintains information about a shader program uniform.  In particular, it
// tracks the type, count and element size of a uniform.  For example,
// an array of 8 vec2s would be described by
//   UniformInfo(UniformInfo::kTypeFloat, 8, 2)
struct UniformInfo {
  enum Type {
    kTypeFloat,
    kTypeInteger,
    kTypeMatrix,
    kTypeUnknown,
  };

  UniformInfo() : type(kTypeUnknown), count(-1), element_size(-1) {}
  UniformInfo(Type type, int count, int element_size)
      : type(type), count(count), element_size(element_size) {}
  bool operator==(const UniformInfo& rhs) const {
    return type == rhs.type && count == rhs.count &&
           element_size == rhs.element_size;
  }
  bool operator!=(const UniformInfo& rhs) const { return !operator==(rhs); }

  // Data type of this uniform.
  Type type;
  // Number of uniforms in an array.
  int count;
  // The "vector size" of the uniform.
  int element_size;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_UNIFORM_INFO_H_
