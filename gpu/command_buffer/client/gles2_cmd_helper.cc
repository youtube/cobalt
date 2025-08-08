// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_cmd_helper.h"

namespace gpu {
namespace gles2 {

GLES2CmdHelper::GLES2CmdHelper(CommandBuffer* command_buffer)
    : CommandBufferHelper(command_buffer) {
}

GLES2CmdHelper::~GLES2CmdHelper() = default;

}  // namespace gles2
}  // namespace gpu



