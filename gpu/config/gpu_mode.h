// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_MODE_H_
#define GPU_CONFIG_GPU_MODE_H_

namespace gpu {

// What the GPU process is running for.
enum class GpuMode {
  UNKNOWN,
  // The GPU process is running with hardware acceleration, using only GL.
  HARDWARE_GL,
  // The GPU process is running with hardware acceleration, using Vulkan and GL.
  HARDWARE_VULKAN,
  // The GPU process is running for SwiftShader WebGL.
  SWIFTSHADER,
  // The GPU process is running for the display compositor.
  DISPLAY_COMPOSITOR,
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_MODE_H_
