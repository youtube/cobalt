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

#ifndef GLIMP_EGL_ERROR_H_
#define GLIMP_EGL_ERROR_H_

#include <EGL/egl.h>

namespace glimp {
namespace egl {

// Implements support for getting and setting the thread local EGL error
// value.
//   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglGetError.xhtml

// Returns the current thread local error code.
EGLint GetError();

// Sets the current thread local error code.
void SetError(EGLint error);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_ERROR_H_
