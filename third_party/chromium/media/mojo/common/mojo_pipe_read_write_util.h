// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_

#include "mojo/public/c/system/types.h"

namespace media_m96 {
namespace mojo_pipe_read_write_util {

bool IsPipeReadWriteError(MojoResult result);

}  // namespace mojo_pipe_read_write_util
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_
