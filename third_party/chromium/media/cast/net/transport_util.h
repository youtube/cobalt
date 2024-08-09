// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_TRANSPORT_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_TRANSPORT_UTIL_H_

#include <string>

#include "base/values.h"

namespace media_m96 {
namespace cast {
namespace transport_util {

// Options for PaceSender.
const char kOptionPacerMaxBurstSize[] = "pacer_max_burst_size";
const char kOptionPacerTargetBurstSize[] = "pacer_target_burst_size";

int LookupOptionWithDefault(const base::DictionaryValue& options,
                            const std::string& path,
                            int default_value);

}  // namespace transport_util
}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_NET_TRANSPORT_UTIL_H_
