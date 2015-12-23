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

#ifndef GLIMP_EGL_DISPLAY_IMPL_H_
#define GLIMP_EGL_DISPLAY_IMPL_H_

#include <EGL/egl.h>

#include "glimp/base/scoped_ptr.h"

namespace glimp {
namespace egl {

// All platform-specific aspects of a EGL Display are implemented within
// subclasses of DisplayImpl.  Platforms must also implement
// DisplayImpl::Create(), declared below, in order to define how
// platform-specific DisplayImpls are to be created.
class DisplayImpl {
 public:
  // Returns true if the given |native_display| is a valid display ID that can
  // be subsequently passed into Create().
  // To be implemented by each implementing platform.
  static bool IsValidNativeDisplayType(EGLNativeDisplayType display_id);
  // Creates and returns a new DisplayImpl object.
  // To be implemented by each implementing platform.
  static base::scoped_ptr<DisplayImpl> Create(EGLNativeDisplayType display_id);

  // Returns the EGL major and minor versions, if they are not NULL.
  // Called by eglInitialize():
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglInitialize.xhtml
  virtual void GetVersionInfo(int* major, int* minor) = 0;

 private:
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_DISPLAY_IMPL_H_
