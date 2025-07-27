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

#include "starboard/shared/starboard/media/drm_session_id_mapper.h"

#include <atomic>

#include "starboard/common/log.h"

namespace starboard::shared::starboard::media {

DrmSessionIdMapper::DrmSessionIdMapper() = default;

std::string DrmSessionIdMapper::GenerateBridgeSessionId() {
  static std::atomic<int> counter = 0;
  return "cobalt.sid." + std::to_string(counter++);
}

std::string_view DrmSessionIdMapper::GetCdmSessionId(
    std::string_view media_drm_session_id) const {
  if (bridge_session_id_map_.has_value() &&
      bridge_session_id_map_->media_drm_id == media_drm_session_id) {
    return bridge_session_id_map_->cdm_id;
  }
  return media_drm_session_id;
}

std::string_view DrmSessionIdMapper::GetMediaDrmSessionId(
    std::string_view cdm_session_id) const {
  if (bridge_session_id_map_.has_value() &&
      bridge_session_id_map_->cdm_id == cdm_session_id) {
    return bridge_session_id_map_->media_drm_id;
  }
  return cdm_session_id;
}

std::string DrmSessionIdMapper::GetBridgeCdmSessionId() {
  if (bridge_session_id_map_.has_value()) {
    SB_LOG(INFO) << __func__ << " re-used exising bridge session id="
                 << bridge_session_id_map_->cdm_id;
    return bridge_session_id_map_->cdm_id;
  }

  bridge_session_id_map_.emplace(
      SessionIdMap{.cdm_id = GenerateBridgeSessionId()});
  SB_LOG(INFO) << __func__ << " created new bridge session: cdm_id="
               << bridge_session_id_map_->cdm_id;
  return bridge_session_id_map_->cdm_id;
}

void DrmSessionIdMapper::RegisterMediaDrmSessionIdIfNotSet(
    std::string_view media_drm_session_id) {
  if (!bridge_session_id_map_.has_value()) {
    SB_LOG(WARNING) << "Cdm session id is not created. Cannot register media "
                       "drm session id="
                    << media_drm_session_id;
    return;
  }
  if (!bridge_session_id_map_->media_drm_id.empty()) {
    return;
  }

  bridge_session_id_map_->media_drm_id = media_drm_session_id;
}

}  // namespace starboard::shared::starboard::media
