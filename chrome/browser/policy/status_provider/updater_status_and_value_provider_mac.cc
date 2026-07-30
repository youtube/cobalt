// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"

#include "base/apple/foundation_util.h"

// static
std::string UpdaterStatusAndValueProvider::GetUpdaterAppId() {
  return std::string(base::apple::BaseBundleID());
}

void UpdaterStatusAndValueProvider::Init() {
  Refresh();
}
