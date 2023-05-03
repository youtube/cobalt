// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/metrics.h"

#include "base/metrics/histogram_macros.h"

namespace variations {

#if defined(OS_ANDROID)
void RecordFirstRunSeedImportResult(FirstRunSeedImportResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.FirstRunResult", result,
                            FirstRunSeedImportResult::ENUM_SIZE);
}
#endif  // OS_ANDROID

void RecordLoadSeedResult(LoadSeedResult state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedLoadResult", state,
                            LoadSeedResult::ENUM_SIZE);
}

void RecordLoadSafeSeedResult(LoadSeedResult state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SafeMode.LoadSafeSeed.Result", state,
                            LoadSeedResult::ENUM_SIZE);
}

void RecordStoreSeedResult(StoreSeedResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedStoreResult", result,
                            StoreSeedResult::ENUM_SIZE);
}

}  // namespace variations
