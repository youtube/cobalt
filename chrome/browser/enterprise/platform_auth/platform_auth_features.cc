// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"

#include "build/build_config.h"

namespace enterprise_auth {

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kCloudApAuthAttachAsHeader,
             "CloudApAuthAttachAsHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_auth
