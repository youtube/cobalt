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

#ifndef GPU_COMMAND_BUFFER_SERVICE_COBALT_PROTECTED_COMMANDS_H_
#define GPU_COMMAND_BUFFER_SERVICE_COBALT_PROTECTED_COMMANDS_H_

#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu::gles2 {

// The GLES2 commands a protected WebGL context may run. Its textures and
// default framebuffer hold decrypted video, so it runs only commands that
// cannot return that content, or values computed from it, to the client.
// Commands that have not been reviewed are refused, so commands added upstream
// stay unavailable to protected contexts until they are classified.
enum class ProtectedCommandPolicy {
  kAllowed,
  // Allowed; the decoder refuses some arguments (see below).
  kAllowedWithArgumentChecks,
  kDenied,
  // Not reviewed yet. Refused like kDenied.
  kUnclassified,
};

GPU_GLES2_EXPORT ProtectedCommandPolicy
GetProtectedCommandPolicy(unsigned int command);

// Whether the decoder may run |command| for a protected context: kAllowed or
// kAllowedWithArgumentChecks.
GPU_GLES2_EXPORT bool IsCommandAllowedInProtectedContext(unsigned int command);

// For BeginQueryEXT. Only queries that report completion are allowed; sample
// counts and GPU times can encode protected texels.
GPU_GLES2_EXPORT bool IsQueryTargetAllowedInProtectedContext(GLenum target);

// For BeginSharedImageAccessDirectCHROMIUM. Writing to a shared image would
// let other contexts read what the protected context drew.
GPU_GLES2_EXPORT bool IsSharedImageAccessModeAllowedInProtectedContext(
    GLenum mode);

}  // namespace gpu::gles2

#endif  // GPU_COMMAND_BUFFER_SERVICE_COBALT_PROTECTED_COMMANDS_H_
