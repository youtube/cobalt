// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/error_state_mock.h"

namespace gpu {
namespace gles2 {

MockErrorState::MockErrorState()
    : ErrorState() {}

MockErrorState::~MockErrorState() = default;

}  // namespace gles2
}  // namespace gpu

