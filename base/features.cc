// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/feature_list.h"

namespace base::features {

// Alphabetical:

// Enforce that writeable file handles passed to untrusted processes are not
// backed by executable files.
BASE_FEATURE(kEnforceNoExecutableFileHandles,
             "EnforceNoExecutableFileHandles",
             FEATURE_ENABLED_BY_DEFAULT);

// Optimizes parsing and loading of data: URLs.
BASE_FEATURE(kOptimizeDataUrls, "OptimizeDataUrls", FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSupportsUserDataFlatHashMap,
             "SupportsUserDataFlatHashMap",
             FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Force to enable LowEndDeviceMode partially on Android mid-range devices.
// Such devices aren't considered low-end, but we'd like experiment with
// a subset of low-end features to see if we get a good memory vs. performance
// tradeoff.
//
// TODO(crbug.com/1434873): |#if| out 32-bit before launching or going to
// high Stable %, because we will enable the feature only for <8GB 64-bit
// devices, where we didn't ship yet. However, we first need a larger
// population to collect data.
BASE_FEATURE(kPartialLowEndModeOnMidRangeDevices,
             "PartialLowEndModeOnMidRangeDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base::features
