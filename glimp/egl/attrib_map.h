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

#ifndef GLIMP_EGL_ATTRIB_MAP_H_
#define GLIMP_EGL_ATTRIB_MAP_H_

#include <EGL/egl.h>

#include <map>

namespace glimp {
namespace egl {

typedef std::map<int, int> AttribMap;

// Many EGL functions take an "attribute list" as a parameter that all share a
// similar format:  A list of integer key/value pairs and concluded with the
// value EGL_NONE (like a null terminated C string).  This function parses
// that attribute list into a map and returns it.
AttribMap ParseRawAttribList(const EGLint* attrib_list);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_ATTRIB_MAP_H_
