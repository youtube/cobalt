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

#include "starboard/android/shared/mock_media_capabilities_cache.h"

namespace starboard {

MockAudioCodecCapability::MockAudioCodecCapability(std::string name,
                                                   bool is_secure_req,
                                                   bool is_secure_sup,
                                                   bool is_tunnel_req,
                                                   bool is_tunnel_sup,
                                                   Range supported_bitrates)
    : AudioCodecCapability(std::move(name),
                           is_secure_req,
                           is_secure_sup,
                           is_tunnel_req,
                           is_tunnel_sup,
                           supported_bitrates) {}

MockVideoCodecCapability::MockVideoCodecCapability(std::string name,
                                                   bool is_secure_req,
                                                   bool is_secure_sup,
                                                   bool is_tunnel_req,
                                                   bool is_tunnel_sup,
                                                   bool is_software_decoder,
                                                   bool is_hdr_capable,
                                                   Range supported_widths,
                                                   Range supported_heights,
                                                   Range supported_bitrates,
                                                   Range supported_frame_rates)

    : VideoCodecCapability(std::move(name),
                           is_secure_req,
                           is_secure_sup,
                           is_tunnel_req,
                           is_tunnel_sup,
                           is_software_decoder,
                           is_hdr_capable,
                           /*j_video_capabilities=*/std::move(nullptr),
                           supported_widths,
                           supported_heights,
                           supported_bitrates,
                           supported_frame_rates) {}

MockMediaCapabilitiesCache::MockMediaCapabilitiesCache(
    std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider)
    : MediaCapabilitiesCache(std::move(media_capabilities_provider)) {}
}  // namespace starboard
