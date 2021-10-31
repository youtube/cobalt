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

#include <set>

#include "glimp/egl/attrib_map.h"
#include "glimp/egl/config.h"
#include "glimp/egl/surface.h"
#include "glimp/gles/context_impl.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace egl {

// All platform-specific aspects of a EGL Display are implemented within
// subclasses of DisplayImpl.  Platforms must also implement
// DisplayImpl::Create(), declared below, in order to define how
// platform-specific DisplayImpls are to be created.
class DisplayImpl {
 public:
  // Return value type for the method GetVersionInfo().
  struct VersionInfo {
    int major;
    int minor;
  };

  typedef std::set<Config*> ConfigSet;

  virtual ~DisplayImpl() {}

  // Returns true if the given |native_display| is a valid display ID that can
  // be subsequently passed into Create().
  // To be implemented by each implementing platform.
  static bool IsValidNativeDisplayType(EGLNativeDisplayType display_id);
  // Creates and returns a new DisplayImpl object.
  // To be implemented by each implementing platform.
  static nb::scoped_ptr<DisplayImpl> Create(EGLNativeDisplayType display_id);

  // Returns the EGL major and minor versions, if they are not NULL.
  // Called by eglInitialize():
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglInitialize.xhtml
  virtual VersionInfo GetVersionInfo() = 0;

  // Returns *all* configs for this display that may be chosen via a call to
  // eglChooseConfig().
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglChooseConfig.xhtml
  virtual const ConfigSet& GetSupportedConfigs() const = 0;

  // Creates and returns a SurfaceImpl object that represents the surface of a
  // window and is compatible with this DisplayImpl object.  This will be called
  // when eglCreateWindowSurface() is called.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglCreateWindowSurface.xhtml
  virtual nb::scoped_ptr<SurfaceImpl> CreateWindowSurface(
      const Config* config,
      EGLNativeWindowType win,
      const AttribMap& attributes) = 0;

  // Creates and returns a SurfaceImpl object that represents the surface of a
  // Pbuffer and is compatible with this DisplayImpl object.  This will be
  // called when eglCreatePbufferSurface() is called.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglCreatePbufferSurface.xhtml
  virtual nb::scoped_ptr<SurfaceImpl> CreatePbufferSurface(
      const Config* config,
      const AttribMap& attributes) = 0;

  // Creates and returns a gles::ContextImpl object that contains the platform
  // specific implementation of a GL ES Context, of the specified version that
  // is compatible with the specified config.
  virtual nb::scoped_ptr<gles::ContextImpl> CreateContext(const Config* config,
                                                          int gles_version) = 0;

  // Sets the swap behavior for this display.  This will be called when
  // eglSwapInterval() is called.  Returns true on success and false on failure.
  //   https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglSwapInterval.xhtml
  virtual bool SetSwapInterval(int interval) = 0;

 private:
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_DISPLAY_IMPL_H_
