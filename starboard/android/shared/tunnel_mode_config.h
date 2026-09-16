// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_ANDROID_SHARED_TUNNEL_MODE_CONFIG_H_
#define STARBOARD_ANDROID_SHARED_TUNNEL_MODE_CONFIG_H_

#include <optional>
#include <ostream>

namespace starboard {

// Encapsulates configuration parameters required for Android tunneled playback.
// An instance of this class is only created when tunnel mode is actively
// enabled and supported for the playback session.
class TunnelModeConfig {
 public:
  TunnelModeConfig(int audio_session_id,
                   bool force_secure_pipeline = false,
                   bool enable_vsp_adjustment = false)
      : audio_session_id_(audio_session_id),
        force_secure_pipeline_(force_secure_pipeline),
        enable_vsp_adjustment_(enable_vsp_adjustment) {}

  int audio_session_id() const { return audio_session_id_; }
  bool force_secure_pipeline() const { return force_secure_pipeline_; }
  bool enable_vsp_adjustment() const { return enable_vsp_adjustment_; }

  friend std::ostream& operator<<(std::ostream& os,
                                  const TunnelModeConfig& config) {
    return os << "TunnelModeConfig{audio_session_id="
              << config.audio_session_id_ << ", force_secure_pipeline="
              << (config.force_secure_pipeline_ ? "true" : "false")
              << ", enable_vsp_adjustment="
              << (config.enable_vsp_adjustment_ ? "true" : "false") << "}";
  }

 private:
  const int audio_session_id_;
  const bool force_secure_pipeline_ = false;
  const bool enable_vsp_adjustment_ = false;
};

inline std::ostream& operator<<(std::ostream& os,
                                const std::optional<TunnelModeConfig>& config) {
  if (!config.has_value()) {
    return os << "TunnelMode: DISABLED";
  }
  return os << *config;
}

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_TUNNEL_MODE_CONFIG_H_
