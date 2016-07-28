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

#include "glimp/egl/display_registry.h"

#include "glimp/egl/display_impl.h"
#include "glimp/egl/error.h"

namespace glimp {
namespace egl {

int DisplayRegistry::num_connections_ = 0;
DisplayRegistry::Connection
    DisplayRegistry::connections_[DisplayRegistry::kMaxDisplays];

EGLDisplay DisplayRegistry::GetDisplay(EGLNativeDisplayType native_display) {
  // Check to see if a display already exists for this native_display.  If so,
  // return it, otherwise create a new one and return that.
  for (int i = 0; i < num_connections_; ++i) {
    if (connections_[i].native_display == native_display) {
      return reinterpret_cast<EGLDisplay>(&connections_[i]);
    }
  }

  // If the platform-specific implementation does not accept the specified
  // |native_display|, return in error.
  if (!DisplayImpl::IsValidNativeDisplayType(native_display)) {
    return EGL_NO_DISPLAY;
  } else {
    // Create a new display connection (i.e. EGLDisplay), add it to our
    // display mapping so it can be looked up later, and then return it.
    SB_CHECK(num_connections_ < kMaxDisplays);
    connections_[num_connections_].native_display = native_display;
    connections_[num_connections_].display = NULL;
    ++num_connections_;
    return reinterpret_cast<EGLDisplay>(&connections_[num_connections_ - 1]);
  }
}

bool DisplayRegistry::InitializeDisplay(EGLDisplay display) {
  SB_DCHECK(Valid(display));
  Connection* connection = reinterpret_cast<Connection*>(display);
  if (!connection->display) {
    nb::scoped_ptr<DisplayImpl> display_impl =
        DisplayImpl::Create(connection->native_display);
    // If the platform-specific glimp implementation rejected the native
    // display, then we return false to indicate failure.
    if (!display_impl) {
      return false;
    }

    connection->display = new Display(display_impl.Pass());
  }

  return true;
}

void DisplayRegistry::TerminateDisplay(EGLDisplay display) {
  SB_DCHECK(Valid(display));
  Connection* connection = reinterpret_cast<Connection*>(display);

  if (connection->display) {
    delete connection->display;
    connection->display = NULL;
  }
}

bool DisplayRegistry::Valid(EGLDisplay display) {
  Connection* connection = reinterpret_cast<Connection*>(display);
  for (int i = 0; i < num_connections_; ++i) {
    if (connection == &connections_[i]) {
      return true;
    }
  }
  return false;
}

// This function will either return the Display object associated with the
// given EGLDisplay, or else set the appropriate EGL error and then return
// NULL.
egl::Display* GetDisplayOrSetError(EGLDisplay egl_display) {
  if (!egl::DisplayRegistry::Valid(egl_display)) {
    egl::SetError(EGL_BAD_DISPLAY);
    return NULL;
  }
  egl::Display* display = egl::DisplayRegistry::ToDisplay(egl_display);
  if (!display) {
    egl::SetError(EGL_NOT_INITIALIZED);
    return NULL;
  }

  return display;
}

}  // namespace egl
}  // namespace glimp
