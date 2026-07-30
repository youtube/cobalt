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

#ifndef STARBOARD_ANDROID_SHARED_DRM_SESSION_ID_MAPPER_H_
#define STARBOARD_ANDROID_SHARED_DRM_SESSION_ID_MAPPER_H_

#include <optional>
#include <string>
#include <string_view>

namespace starboard {

// Manages the mapping between a EME session ID and a MediaDrm session ID.
// This class is thread-compatible. It is not internally thread-safe. The owner
// of this class should handle locking to ensure thread-safety when the same
// instance is accessed from multiple threads.
class DrmSessionIdMapper {
 public:
  DrmSessionIdMapper();

  // Gets the EME session ID corresponding to the given |media_drm_session_id|.
  // If no mapping is found, returns the input |media_drm_session_id| itself.
  std::string_view GetEmeSessionId(std::string_view media_drm_session_id) const;
  // Gets the MediaDrm session ID corresponding to the given |eme_session_id|.
  // This method has two special cases:
  // 1. For a bridge session, it may return an empty string if the MediaDrm
  //    session has not yet been established.
  // 2. If no mapping is found, it returns the input |eme_session_id| itself.
  std::string_view GetMediaDrmSessionId(std::string_view eme_session_id) const;

  std::string CreateOrGetBridgeEmeSessionId();
  bool IsMediaDrmSessionIdForProvisioningRequired() const;
  void RegisterMediaDrmSessionIdForProvisioning(
      std::string_view media_drm_session_id);

 private:
  struct SessionIdMap {
    const std::string eme_id;
    std::string media_drm_id;
  };
  // Session id map for bridge session, which is created before MediaDrm session
  // is created in EME session (e.g. when device is not provisioned).
  std::optional<SessionIdMap> bridge_session_id_map_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_SESSION_ID_MAPPER_H_
