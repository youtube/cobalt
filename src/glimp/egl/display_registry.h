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

#ifndef GLIMP_EGL_DISPLAY_REGISTRY_H_
#define GLIMP_EGL_DISPLAY_REGISTRY_H_

#include "glimp/egl/display.h"
#include "starboard/common/log.h"

namespace glimp {
namespace egl {

// Maintains a registry of EGLDisplays as well as mappings from
// EGLNativeDisplayType to those EGLDisplays.  Internally, an EGLDisplay is
// represented as a DisplayRegistry::Connection object that has a possibly null
// pointer to a Display object.  The Display object will be initialized when
// DisplayRegistry::InitializeDisplay() is called, after which it may be
// retrieved and have methods called on it.
class DisplayRegistry {
 public:
  // Looks up the mapping from EGLNativeDisplayType to EGLDisplay, or creates
  // one if it doesn't already exist, and returns the EGLDisplay associated
  // with it.  No failures will be reported by this method, it is
  // InitializeDisplay() that will ultimately have the platform check
  // whether the native display is valid or not.
  static EGLDisplay GetDisplay(EGLNativeDisplayType native_display);

  // Construct the Display object, if it has not yet been initialized already.
  // This method can fail if the native display associated with the EGLDisplay
  // is rejected by the platform.  Returns true if the display is already
  // initialized or successfully initialized, otherwise returns false.
  static bool InitializeDisplay(EGLDisplay display);

  // Terminates the display if it is initialized, otherwise it does nothing.
  static void TerminateDisplay(EGLDisplay display);

  // Returns true if the given |display| is valid, which will be true if it
  // was at some point previously returned by GetDisplay() and false otherwise.
  static bool Valid(EGLDisplay display);

  // Returns the Display object associated with the given [valid] EGLDisplay
  // object, or NULL if it is not initialized.
  static Display* ToDisplay(EGLDisplay display) {
    SB_DCHECK(Valid(display));
    return reinterpret_cast<Connection*>(display)->display;
  }

 private:
  // A Connection is the internal type of a EGLDisplay handle.  It
  // maintains a mapping from EGLNativeDisplayType to a (possibly null if it
  // hasn't yet been eglInitialize()d) Display object.
  struct Connection {
    EGLNativeDisplayType native_display;
    Display* display;
  };

  static const int kMaxDisplays = 10;

  // The number of display connections currently active.
  static int num_connections_;

  // The mapping from native type to possibly initialized connection.
  static Connection connections_[kMaxDisplays];
};

// This function will either return the Display object associated with the
// given EGLDisplay, or else set the appropriate EGL error and then return
// NULL.
egl::Display* GetDisplayOrSetError(EGLDisplay egl_display);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_DISPLAY_REGISTRY_H_
