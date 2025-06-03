// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace data_controls {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDataControlsRulesPref);
}

}  // namespace data_controls
