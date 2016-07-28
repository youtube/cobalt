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

#include "glimp/egl/error.h"

#include "starboard/once.h"
#include "starboard/thread.h"

namespace glimp {
namespace egl {

namespace {
SbOnceControl s_error_once_control = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_error_tls_key = kSbThreadLocalKeyInvalid;

void InitializeError() {
  s_error_tls_key = SbThreadCreateLocalKey(NULL);
}
}  // namespace

EGLint GetError() {
  SbOnce(&s_error_once_control, &InitializeError);
  void* local_value = SbThreadGetLocalValue(s_error_tls_key);
  if (local_value == NULL) {
    // The EGL error has never been set.  In this case, return EGL_SUCCESS as
    // that is the initial value for eglGetError().
    // Note that NULL or 0 are not valid EGL error codes.
    // https://www.khronos.org/registry/egl/sdk/docs/man/html/eglGetError.xhtml
    return EGL_SUCCESS;
  }
  return static_cast<EGLint>(reinterpret_cast<uintptr_t>(local_value));
}

void SetError(EGLint error) {
  SbOnce(&s_error_once_control, &InitializeError);
  SbThreadSetLocalValue(s_error_tls_key, reinterpret_cast<void*>(error));
}

}  // namespace egl
}  // namespace glimp
