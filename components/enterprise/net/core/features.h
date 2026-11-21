// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_

#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

#include "base/feature_list.h"

namespace enterprise_net {

// Controls whether dynamic route fetching is enabled.
BASE_DECLARE_FEATURE(kEnableDynamicRouteFetching);

// Return true if dynamic route fetching is enabled.
bool IsDynamicRouteFetchingEnabled();

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_
