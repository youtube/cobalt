// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_
#define COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_

#include <stdint.h>

#include "base/functional/callback.h"

struct wl_client;

namespace exo {
namespace wayland {

void bind_remote_shell_v2(wl_client* client,
                          void* data,
                          uint32_t version,
                          uint32_t id);
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZCR_REMOTE_SHELL_V2_H_
