/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/network_monitor.h"

#include "absl/strings/string_view.h"

namespace webrtc {

absl::string_view NetworkPreferenceToString(NetworkPreference preference) {
  switch (preference) {
    case NetworkPreference::NEUTRAL:
      return "NEUTRAL";
    case NetworkPreference::NOT_PREFERRED:
      return "NOT_PREFERRED";
  }
}

absl::string_view NetworkSliceToString(NetworkSlice network_slice) {
  switch (network_slice) {
    case NetworkSlice::NO_SLICE:
      return "NO_SLICE";
    case NetworkSlice::UNIFIED_COMMUNICATIONS:
      return "UNIFIED_COMMUNICATIONS";
  }
}

NetworkMonitorInterface::NetworkMonitorInterface() {}
NetworkMonitorInterface::~NetworkMonitorInterface() {}

}  // namespace webrtc
