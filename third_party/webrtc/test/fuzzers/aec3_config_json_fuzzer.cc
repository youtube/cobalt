/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>

#include "absl/strings/string_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/test/echo_canceller3_config_json.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 10'000) {
    return;
  }
  absl::string_view config_json = fuzz_data.ReadString();

  EchoCanceller3Config config;
  bool success;
  Aec3ConfigFromJsonString(config_json, &config, &success);
  EchoCanceller3Config::Validate(&config);
}

}  // namespace webrtc
