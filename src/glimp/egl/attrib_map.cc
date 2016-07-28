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

#include "glimp/egl/attrib_map.h"

namespace glimp {
namespace egl {

AttribMap ParseRawAttribList(const EGLint* attrib_list) {
  AttribMap ret;
  if (!attrib_list) {
    return ret;
  }

  const int* current_attrib = attrib_list;
  while (*current_attrib != EGL_NONE) {
    int key = *current_attrib++;
    int value = *current_attrib++;

    ret[key] = value;
  }

  return ret;
}

}  // namespace egl
}  // namespace glimp
