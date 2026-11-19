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

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_ROBUST_FALLBACKS_COBALT_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_ROBUST_FALLBACKS_COBALT_H_

namespace gl {
struct DriverGL;
}  // namespace gl

namespace gpu {
namespace gles2 {

// Cobalt on some platforms (like RDK) runs the passthrough command decoder on
// a native GLES driver without ANGLE. The passthrough decoder calls the
// GL_ANGLE_robust_client_memory entry points (gl*RobustANGLE) unconditionally,
// which are null without ANGLE. This installs implementations of the missing
// entry points on top of the regular GLES ones: they bound the amount of data
// written and read to |bufSize| and report the number of values written in
// |length|, as the robust versions do. Entry points the driver already
// provides are left untouched.
void InstallRobustANGLEFallbacksIfNeeded(gl::DriverGL* driver);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_ROBUST_FALLBACKS_COBALT_H_
