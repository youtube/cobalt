// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_developer_mode_only.h"

#include <map>

#include "base/no_destructor.h"

namespace {

// A map between a profile (referenced by a unique id) and the current developer
// mode for that profile. Since different profiles have different developer
// modes, we need to have separate entries.
using CurrentDeveloperModeMap = std::map<int, bool>;

CurrentDeveloperModeMap& GetDeveloperModeMap() {
  static base::NoDestructor<CurrentDeveloperModeMap> map;
  return *map;
}

}  // namespace

namespace extensions {

// Returns the current developer mode for the given context_id.
bool GetCurrentDeveloperMode(int context_id) {
  CurrentDeveloperModeMap& map = GetDeveloperModeMap();
  auto iter = map.find(context_id);
  return iter == map.end() ? false : iter->second;
}

// static
void SetCurrentDeveloperMode(int context_id, bool current_developer_mode) {
  GetDeveloperModeMap()[context_id] = current_developer_mode;
}

}  // namespace extensions
