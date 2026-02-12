// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media/drm_session_id_mapper.h"

#include <atomic>
#include <string_view>

#include "starboard/common/log.h"

namespace starboard {

namespace {
std::string GenerateBridgeSessionId() {
  static std::atomic<int> counter = 0;
  return "cobalt.sid." + std::to_string(counter++);
}
}  // namespace

DrmSessionIdMapper::DrmSessionIdMapper() = default;

std::string_view DrmSessionIdMapper::GetEmeSessionId(
    std::string_view media_drm_session_id) const {
  if (bridge_session_id_map_.has_value() &&
      bridge_session_id_map_->media_drm_id == media_drm_session_id) {
    return bridge_session_id_map_->eme_id;
  }
  return media_drm_session_id;
}

std::string_view DrmSessionIdMapper::GetMediaDrmSessionId(
    std::string_view eme_session_id) const {
  if (bridge_session_id_map_.has_value() &&
      bridge_session_id_map_->eme_id == eme_session_id) {
    return bridge_session_id_map_->media_drm_id;
  }
  return eme_session_id;
}

std::string DrmSessionIdMapper::CreateOrGetBridgeEmeSessionId() {
  if (bridge_session_id_map_.has_value()) {
    SB_LOG(INFO) << __func__ << " re-used exising bridge session id="
                 << bridge_session_id_map_->eme_id;
    return bridge_session_id_map_->eme_id;
  }

  bridge_session_id_map_.emplace(
      SessionIdMap{.eme_id = GenerateBridgeSessionId()});
  SB_LOG(INFO) << __func__ << " created new bridge session: eme_id="
               << bridge_session_id_map_->eme_id;
  return bridge_session_id_map_->eme_id;
}

bool DrmSessionIdMapper::IsMediaDrmSessionIdForProvisioningRequired() const {
  return bridge_session_id_map_.has_value() &&
         bridge_session_id_map_->media_drm_id.empty();
}

void DrmSessionIdMapper::RegisterMediaDrmSessionIdForProvisioning(
    std::string_view media_drm_session_id) {
  SB_CHECK(IsMediaDrmSessionIdForProvisioningRequired());
  bridge_session_id_map_->media_drm_id = media_drm_session_id;
}

}  // namespace starboard
