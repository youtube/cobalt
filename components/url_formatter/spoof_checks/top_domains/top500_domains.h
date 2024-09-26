// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP500_DOMAINS_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP500_DOMAINS_H_

#include <cstddef>

namespace top500_domains {

extern const char* const kTop500EditDistanceSkeletons[];
extern const size_t kNumTop500EditDistanceSkeletons;

extern const char* const kTopKeywords[];
extern const size_t kNumTopKeywords;

}  // namespace top500_domains

#endif  //  COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TOP500_DOMAINS_H_
