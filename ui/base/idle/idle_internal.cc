// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle_internal.h"

namespace ui {

absl::optional<IdleState>& IdleStateForTesting() {
  static absl::optional<IdleState> idle_state;
  return idle_state;
}

}  // namespace ui
