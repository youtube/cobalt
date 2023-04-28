// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/cellular_logic_helper.h"

#include "net/base/network_change_notifier.h"

namespace metrics {

namespace {

// Standard interval between log uploads, in seconds.
#if defined(OS_ANDROID) || defined(OS_IOS)
const int kStandardUploadIntervalSeconds = 5 * 60;           // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

#if defined(OS_ANDROID)
const bool kDefaultCellularLogicEnabled = true;
#else
const bool kDefaultCellularLogicEnabled = false;
#endif

}  // namespace

base::TimeDelta GetUploadInterval() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  if (IsCellularLogicEnabled())
    return base::TimeDelta::FromSeconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

// Returns true if current connection type is cellular and cellular logic is
// enabled.
bool IsCellularLogicEnabled() {
  if (!kDefaultCellularLogicEnabled)
    return false;

  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

}  // namespace metrics
