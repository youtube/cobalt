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

#ifndef GLIMP_SHADERS_GLSL_SHADER_MAP_HELPERS_H_
#define GLIMP_SHADERS_GLSL_SHADER_MAP_HELPERS_H_

#include <map>

#include "starboard/types.h"

namespace glimp {
namespace shaders {

struct ShaderData {
  ShaderData() : data(NULL), size(0) {}
  ShaderData(const uint8_t* data, size_t size) : data(data), size(size) {}

  const uint8_t* data;
  size_t size;
};

typedef std::map<uint32_t, ShaderData> GLSLShaderHashMap;

}  // namespace shaders
}  // namespace glimp

#endif  // GLIMP_SHADERS_GLSL_SHADER_MAP_HELPERS_H_
