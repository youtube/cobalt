// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/group_constants.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace feature_engagement {

// Group-related features used by the In-Product Help system.

BASE_FEATURE(kIPHGroups, "IPHGroups", base::FEATURE_DISABLED_BY_DEFAULT);

// Group features used by various clients to control their In-Product Help
// groups.

BASE_FEATURE(kIPHDummyGroup,
             "IPH_DummyGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kiOSFullscreenPromosGroup,
             "IPH_iOSFullscreenPromosGroup",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace feature_engagement
