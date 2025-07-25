// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_

#include "base/feature_list.h"
#include "components/gwp_asan/client/export.h"

namespace gwp_asan::internal {

GWP_ASAN_EXPORT BASE_DECLARE_FEATURE(kGwpAsanMalloc);
GWP_ASAN_EXPORT BASE_DECLARE_FEATURE(kGwpAsanPartitionAlloc);

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_
