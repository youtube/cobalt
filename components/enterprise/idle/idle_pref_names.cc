// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/idle_pref_names.h"

namespace enterprise_idle::prefs {
// Number of minutes of inactivity before running actions from
// kIdleTimeoutActions. Controlled via the IdleTimeout policy.
const char kIdleTimeout[] = "idle_timeout";

// Actions to run when the idle timeout is reached. Controller via the
// IdleTimeoutActions policy.
const char kIdleTimeoutActions[] = "idle_timeout_actions";

// If true, show the IdleTimeout bubble when Chrome starts.
const char kIdleTimeoutShowBubbleOnStartup[] =
    "idle_timeout_show_bubble_on_startup";
}  // namespace enterprise_idle::prefs
