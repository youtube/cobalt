// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/gl_ozone_egl_starboard.h"

#include <cstring>

#include "starboard/egl.h"
#include "starboard/gles.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/platform/starboard/surface_factory_starboard.h"
#include "ui/ozone/public/gl_ozone.h"

namespace ui {

namespace {

SbEglBoolean FakeMakeCurrent(SbEglDisplay display,
                             SbEglSurface draw,
                             SbEglSurface read,
                             SbEglContext context) {
  return SB_EGL_TRUE;
}

void FakeEglExtensionProc() {}

// Only knows an extension entry point that isn't in the SbEglInterface.
SbEglCastsToProperFunctionPointerType FakeGetProcAddress(const char* name) {
  if (strcmp(name, "eglQueryDmaBufFormatsEXT") == 0) {
    return &FakeEglExtensionProc;
  }
  return nullptr;
}

void FakeGlClear(SbGlBitfield mask) {}

}  // namespace

// Verifies that Starboard EGL client extension parsing matches the exact
// EGL_ANGLE_platform_angle token, and rejects null, empty, or prefix-only
// matches, so that Cobalt doesn't run ANGLE on top of a Starboard ANGLE.
TEST(GLOzoneEGLStarboardTest, DetectsAngleClientExtension) {
  EXPECT_FALSE(GLOzoneEGLStarboard::HasAnglePlatformExtension(nullptr));
  EXPECT_FALSE(GLOzoneEGLStarboard::HasAnglePlatformExtension(""));
  EXPECT_FALSE(GLOzoneEGLStarboard::HasAnglePlatformExtension(
      "EGL_EXT_client_extensions EGL_ANGLE_platform_angle_vulkan"));
  EXPECT_TRUE(GLOzoneEGLStarboard::HasAnglePlatformExtension(
      "EGL_ANGLE_platform_angle"));
  EXPECT_TRUE(GLOzoneEGLStarboard::HasAnglePlatformExtension(
      "EGL_EXT_client_extensions EGL_ANGLE_platform_angle EGL_KHR_debug"));
}

// Verifies the Starboard EGL/GLES entry point lookup: SbEglInterface first,
// then eglGetProcAddress(), then SbGlesInterface, with missing interfaces and
// names handled safely.
TEST(GLOzoneEGLStarboardTest, ProcAddressFallbackOrder) {
  SbEglInterface egl = {};
  egl.eglMakeCurrent = &FakeMakeCurrent;
  egl.eglGetProcAddress = &FakeGetProcAddress;
  SbGlesInterface gles = {};
  gles.glClear = &FakeGlClear;

  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress("eglMakeCurrent",
                                                             &egl, &gles),
            reinterpret_cast<gl::GLFunctionPointerType>(&FakeMakeCurrent));
  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress(
                "eglQueryDmaBufFormatsEXT", &egl, &gles),
            reinterpret_cast<gl::GLFunctionPointerType>(&FakeEglExtensionProc));
  EXPECT_EQ(
      GLOzoneEGLStarboard::ResolveStarboardProcAddress("glClear", &egl, &gles),
      reinterpret_cast<gl::GLFunctionPointerType>(&FakeGlClear));
  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress("glUnknownProc",
                                                             &egl, &gles),
            nullptr);

  // Missing name or interfaces.
  EXPECT_EQ(
      GLOzoneEGLStarboard::ResolveStarboardProcAddress(nullptr, &egl, &gles),
      nullptr);
  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress("eglMakeCurrent",
                                                             nullptr, nullptr),
            nullptr);
  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress("glClear", nullptr,
                                                             &gles),
            reinterpret_cast<gl::GLFunctionPointerType>(&FakeGlClear));

  // An SbEglInterface without eglGetProcAddress.
  egl.eglGetProcAddress = nullptr;
  EXPECT_EQ(GLOzoneEGLStarboard::ResolveStarboardProcAddress(
                "eglQueryDmaBufFormatsEXT", &egl, &gles),
            nullptr);
}

// Verifies that GLOzoneEGLStarboard loads the static GL/EGL bindings with the
// platform's Starboard EGL implementation and resolves core EGL entry points.
TEST(GLOzoneEGLStarboardTest, InitializeStaticGLBindings) {
  const SbEglInterface* egl = SbGetEglInterface();
  if (!egl || !egl->eglGetProcAddress) {
    GTEST_SKIP() << "SbEglInterface unavailable in this test environment.";
  }

  const gl::GLImplementationParts implementation(gl::kGLImplementationEGLANGLE);
  SurfaceFactoryStarboard factory;
  GLOzone* gl_ozone = factory.GetGLOzone(implementation);
  ASSERT_NE(gl_ozone, nullptr);

  ASSERT_TRUE(gl_ozone->InitializeStaticGLBindings(implementation));
  EXPECT_NE(gl::GetGLProcAddress("eglGetError"), nullptr);

  gl_ozone->ShutdownGL(nullptr);
}

}  // namespace ui
