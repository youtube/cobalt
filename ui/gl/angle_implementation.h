// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANGLE_IMPLEMENTATION_H_
#define UI_GL_ANGLE_IMPLEMENTATION_H_

namespace gl {

enum class ANGLEImplementation {
  kNone = 0,
  kD3D11 = 1,
  kOpenGL = 2,
  kOpenGLES = 3,
  kNull = 4,
  kVulkan = 5,
  kSwiftShader = 6,
  kMetal = 7,
  kDefault = 8,
  kD3D11Warp = 9,
  kMaxValue = kD3D11Warp,
};

}  // namespace gl

#endif  // UI_GL_ANGLE_IMPLEMENTATION_H_
