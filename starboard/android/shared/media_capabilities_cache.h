// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_

#include <jni.h>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: encapsulate a common Range class.
struct Range {
  Range() : minimum(0), maximum(0) {}
  Range(int min, int max) : minimum(min), maximum(max) {}
  int minimum;
  int maximum;

  bool Contains(int val) const { return val >= minimum && val <= maximum; }
};

class CodecCapability {
 public:
  CodecCapability(JniEnvExt* env, jobject j_codec_info);
  virtual ~CodecCapability() {}

  const std::string& name() const { return name_; }
  bool is_secure_required() const { return is_secure_required_; }
  bool is_secure_supported() const { return is_secure_supported_; }
  bool is_tunnel_mode_required() const { return is_tunnel_mode_required_; }
  bool is_tunnel_mode_supported() const { return is_tunnel_mode_supported_; }

 private:
  CodecCapability(const CodecCapability&) = delete;
  CodecCapability& operator=(const CodecCapability&) = delete;

  std::string name_;
  bool is_secure_required_;
  bool is_secure_supported_;
  bool is_tunnel_mode_required_;
  bool is_tunnel_mode_supported_;
};

class AudioCodecCapability : public CodecCapability {
 public:
  AudioCodecCapability(JniEnvExt* env,
                       jobject j_codec_info,
                       jobject j_audio_capabilities);
  ~AudioCodecCapability() override {}

  bool IsBitrateSupported(int bitrate) const;

 private:
  AudioCodecCapability(const AudioCodecCapability&) = delete;
  AudioCodecCapability& operator=(const AudioCodecCapability&) = delete;

  Range supported_bitrates_;
};

class VideoCodecCapability : public CodecCapability {
 public:
  VideoCodecCapability(JniEnvExt* env,
                       jobject j_codec_info,
                       jobject j_video_capabilities);
  ~VideoCodecCapability() override;

  bool is_software_decoder() const { return is_software_decoder_; }
  bool is_hdr_capable() const { return is_hdr_capable_; }

  bool IsBitrateSupported(int bitrate) const;
  // VideoCodecCapability caches java object MediaCodecInfo.VideoCapabilities.
  // If improved support check is used,
  // VideoCapabilities.areSizeAndRateSupported() or
  // VideoCapabilities.isSizeSupported() will be used to check the
  // supportability.
  bool AreResolutionAndRateSupported(bool force_improved_support_check,
                                     int frame_width,
                                     int frame_height,
                                     int fps);

 private:
  VideoCodecCapability(const VideoCodecCapability&) = delete;
  VideoCodecCapability& operator=(const VideoCodecCapability&) = delete;

  bool is_software_decoder_;
  bool is_hdr_capable_;
  jobject j_video_capabilities_;
  Range supported_widths_;
  Range supported_heights_;
  Range supported_bitrates_;
  Range supported_frame_rates_;
};

class MediaCapabilitiesCache {
 public:
  static MediaCapabilitiesCache* GetInstance();

  bool IsWidevineSupported();
  bool IsCbcsSchemeSupported();

  bool IsHDRTransferCharacteristicsSupported(SbMediaTransferId transfer_id);

  bool IsPassthroughSupported(SbMediaAudioCodec codec);

  int GetMaxAudioOutputChannels();

  bool HasAudioDecoderFor(const std::string& mime_type,
                          int bitrate,
                          bool must_support_tunnel_mode);

  bool HasVideoDecoderFor(const std::string& mime_type,
                          bool must_support_secure,
                          bool must_support_hdr,
                          bool must_support_tunnel_mode,
                          bool force_improved_support_check,
                          int frame_width,
                          int frame_height,
                          int bitrate,
                          int fps);

  std::string FindAudioDecoder(const std::string& mime_type,
                               int bitrate,
                               bool must_support_tunnel_mode);

  std::string FindVideoDecoder(const std::string& mime_type,
                               bool must_support_secure,
                               bool must_support_hdr,
                               bool require_software_codec,
                               bool must_support_tunnel_mode,
                               bool force_improved_support_check,
                               int frame_width,
                               int frame_height,
                               int bitrate,
                               int fps);

  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }
  void ClearCache();

  void ReloadSupportedHdrTypes();
  void ReloadAudioOutputChannels();

 private:
  MediaCapabilitiesCache();
  ~MediaCapabilitiesCache() {}

  MediaCapabilitiesCache(const MediaCapabilitiesCache&) = delete;
  MediaCapabilitiesCache& operator=(const MediaCapabilitiesCache&) = delete;

  void LazyInitialize_Locked();
  void LoadCodecInfos_Locked();

  Mutex mutex_;

  std::set<SbMediaTransferId> supported_transfer_ids_;
  std::map<SbMediaAudioCodec, bool> passthrough_supportabilities_;

  typedef std::vector<std::unique_ptr<AudioCodecCapability>>
      AudioCodecCapabilities;
  typedef std::vector<std::unique_ptr<VideoCodecCapability>>
      VideoCodecCapabilities;

  std::map<std::string, AudioCodecCapabilities> audio_codec_capabilities_map_;
  std::map<std::string, VideoCodecCapabilities> video_codec_capabilities_map_;

  std::atomic_bool is_enabled_{true};
  bool is_initialized_ = false;
  bool is_widevine_supported_ = false;
  bool is_cbcs_supported_ = false;
  int max_audio_output_channels_ = -1;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_
