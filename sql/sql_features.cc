// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

namespace sql {

namespace features {

// Enable WAL mode for all SQLite databases.
BASE_FEATURE(kEnableWALModeByDefault,
             "EnableWALModeByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

}  // namespace sql
