// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/features.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_net {

BASE_FEATURE(kEnableDynamicRouteFetching, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDynamicRouteFetchingEnabled() {
  return base::FeatureList::IsEnabled(kEnableDynamicRouteFetching);
}

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)
