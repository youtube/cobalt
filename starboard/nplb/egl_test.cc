// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/egl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Verifies that we are able to query for the Starboard EGL interface, and that
// the interface is correctly populated with function pointers that cover the
// EGL 1.4 interface.

TEST(SbEglInterfaceTest, HasValidEglInterface) {
  const SbEglInterface* const egl_interface = SbGetEglInterface();

  if (!egl_interface) {
    return;
  }

  EXPECT_NE(nullptr, egl_interface->eglChooseConfig);
  EXPECT_NE(nullptr, egl_interface->eglCopyBuffers);
  EXPECT_NE(nullptr, egl_interface->eglCreateContext);
  EXPECT_NE(nullptr, egl_interface->eglCreatePbufferSurface);
  EXPECT_NE(nullptr, egl_interface->eglCreatePixmapSurface);
  EXPECT_NE(nullptr, egl_interface->eglCreateWindowSurface);
  EXPECT_NE(nullptr, egl_interface->eglDestroyContext);
  EXPECT_NE(nullptr, egl_interface->eglDestroySurface);
  EXPECT_NE(nullptr, egl_interface->eglGetConfigAttrib);
  EXPECT_NE(nullptr, egl_interface->eglGetConfigs);
  EXPECT_NE(nullptr, egl_interface->eglGetCurrentDisplay);
  EXPECT_NE(nullptr, egl_interface->eglGetCurrentSurface);
  EXPECT_NE(nullptr, egl_interface->eglGetDisplay);
  EXPECT_NE(nullptr, egl_interface->eglGetError);
  EXPECT_NE(nullptr, egl_interface->eglGetProcAddress);
  EXPECT_NE(nullptr, egl_interface->eglInitialize);
  EXPECT_NE(nullptr, egl_interface->eglMakeCurrent);
  EXPECT_NE(nullptr, egl_interface->eglQueryContext);
  EXPECT_NE(nullptr, egl_interface->eglQueryString);
  EXPECT_NE(nullptr, egl_interface->eglQuerySurface);
  EXPECT_NE(nullptr, egl_interface->eglSwapBuffers);
  EXPECT_NE(nullptr, egl_interface->eglTerminate);
  EXPECT_NE(nullptr, egl_interface->eglWaitGL);
  EXPECT_NE(nullptr, egl_interface->eglWaitNative);
  EXPECT_NE(nullptr, egl_interface->eglBindTexImage);
  EXPECT_NE(nullptr, egl_interface->eglReleaseTexImage);
  EXPECT_NE(nullptr, egl_interface->eglSurfaceAttrib);
  EXPECT_NE(nullptr, egl_interface->eglSwapInterval);
  EXPECT_NE(nullptr, egl_interface->eglBindAPI);
  EXPECT_NE(nullptr, egl_interface->eglQueryAPI);
  EXPECT_NE(nullptr, egl_interface->eglCreatePbufferFromClientBuffer);
  EXPECT_NE(nullptr, egl_interface->eglReleaseThread);
  EXPECT_NE(nullptr, egl_interface->eglWaitClient);
  EXPECT_NE(nullptr, egl_interface->eglGetCurrentContext);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
