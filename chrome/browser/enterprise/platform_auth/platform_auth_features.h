// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace enterprise_auth {

#if BUILDFLAG(IS_WIN)
// Controls whether ambient authentication using the CloudAP framework is
// enabled.
BASE_DECLARE_FEATURE(kCloudApAuth);

// Determines whether authentication data beginning with 'x-ms-' should be added
// to requests as a header instead of a cookie.
BASE_DECLARE_FEATURE(kCloudApAuthAttachAsHeader);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
