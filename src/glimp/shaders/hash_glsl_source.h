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

#ifndef GLIMP_SHADERS_HASH_GLSL_SOURCE_H_
#define GLIMP_SHADERS_HASH_GLSL_SOURCE_H_

#include <KHR/khrplatform.h>

namespace glimp {
namespace shaders {

// This function takes a GLSL source and computes a hash value to represent it.
// This hash function is the exact same the one implemented in Python in
// generate_glsl_shader_map.py (the function HashGLSLShaderFile()) so that
// hashes by that script at build time can be linked up at runtime with this
// function.
//
// Note, all space, newline and tab characters are ignored when computing the
// hash.
//
// The hashing source is taken from the "one_at_a_time" hash function, by Bob
// Jenkins, in the public domain at http://burtleburtle.net/bob/hash/doobs.html.
uint32_t HashGLSLSource(const char* source);

}  // namespace shaders
}  // namespace glimp

#endif  // GLIMP_SHADERS_HASH_GLSL_SOURCE_H_
