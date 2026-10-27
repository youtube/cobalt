/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "absl/strings/string_view.h"
#include "api/transport/stun.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  absl::string_view message = fuzz_data.ReadString();

  webrtc::StunMessage::ValidateFingerprint(message.data(), message.size());
  webrtc::StunMessage::ValidateMessageIntegrityForTesting(message.data(),
                                                          message.size(), "");
}
}  // namespace webrtc
