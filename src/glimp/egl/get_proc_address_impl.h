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

#ifndef GLIMP_EGL_GET_PROC_ADDRESS_IMPL_H_
#define GLIMP_EGL_GET_PROC_ADDRESS_IMPL_H_

namespace glimp {
namespace egl {

typedef void (*MustCastToProperFunctionPointerType)(void);

// Calls to eglGetProcAddress() that are unhandled by platform-independent
// glimp code are forwarded to this platform-specific call, so that different
// platforms can implement custom extensions.  This function should return
// a function pointer to the requested function upon success, and return NULL
// if the requested function is not available.
MustCastToProperFunctionPointerType GetProcAddressImpl(const char* procname);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_GET_PROC_ADDRESS_IMPL_H_
