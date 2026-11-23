// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/features.h"

namespace context_hub::features {

BASE_FEATURE(kContextHub, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutoTodos, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAutoTodosTimeoutSeconds,
                   &kAutoTodos,
                   "timeout_seconds",
                   30);

BASE_FEATURE(kMemoryBanks, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace context_hub::features
