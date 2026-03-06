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
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {

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
  CodecCapability(JNIEnv* env,
                  base::android::ScopedJavaLocalRef<jobject>& j_codec_info);
  virtual ~CodecCapability() {}

  const std::string& name() const { return name_; }
  bool is_secure_required() const { return is_secure_required_; }
  bool is_secure_supported() const { return is_secure_supported_; }
  bool is_tunnel_mode_required() const { return is_tunnel_mode_required_; }
  bool is_tunnel_mode_supported() const { return is_tunnel_mode_supported_; }

 protected:
  CodecCapability(std::string name,
                  bool is_secure_req,
                  bool is_secure_sup,
                  bool is_tunnel_req,
                  bool is_tunnel_sup);

 private:
  CodecCapability(const CodecCapability&) = delete;
  CodecCapability& operator=(const CodecCapability&) = delete;

  const std::string name_;
  const bool is_secure_required_;
  const bool is_secure_supported_;
  const bool is_tunnel_mode_required_;
  const bool is_tunnel_mode_supported_;
};

class AudioCodecCapability : public CodecCapability {
 public:
  AudioCodecCapability(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobject>& j_codec_info,
      base::android::ScopedJavaLocalRef<jobject>& j_audio_capabilities);
  ~AudioCodecCapability() override {}

  bool IsBitrateSupported(int bitrate) const;

 protected:
  AudioCodecCapability(std::string name,
                       bool is_secure_req,
                       bool is_secure_sup,
                       bool is_tunnel_req,
                       bool is_tunnel_sup,
                       Range supported_bitrates);

 private:
  AudioCodecCapability(const AudioCodecCapability&) = delete;
  AudioCodecCapability& operator=(const AudioCodecCapability&) = delete;

  const Range supported_bitrates_;
};

class VideoCodecCapability : public CodecCapability {
 public:
  VideoCodecCapability(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobject>& j_codec_info,
      base::android::ScopedJavaLocalRef<jobject>& j_video_capabilities);
  ~VideoCodecCapability() override {}

  bool is_software_decoder() const { return is_software_decoder_; }
  bool is_hdr_capable() const { return is_hdr_capable_; }

  bool IsBitrateSupported(int bitrate) const;
  // VideoCodecCapability caches java object MediaCodecInfo.VideoCapabilities.
  // If improved support check is used,
  // VideoCapabilities.areSizeAndRateSupported() or
  // VideoCapabilities.isSizeSupported() will be used to check the
  // supportability.
  bool AreResolutionAndRateSupported(int frame_width,
                                     int frame_height,
                                     int fps) const;

 protected:
  VideoCodecCapability(std::string name,
                       bool is_secure_req,
                       bool is_secure_sup,
                       bool is_tunnel_req,
                       bool is_tunnel_sup,
                       bool is_software_decoder,
                       bool is_hdr_capable,
                       base::android::ScopedJavaGlobalRef<jobject> j_video_cap,
                       Range supported_widths,
                       Range supported_heights,
                       Range supported_bitrates,
                       Range supported_frame_rates);

 private:
  VideoCodecCapability(const VideoCodecCapability&) = delete;
  VideoCodecCapability& operator=(const VideoCodecCapability&) = delete;

  const bool is_software_decoder_;
  const bool is_hdr_capable_;
  const base::android::ScopedJavaGlobalRef<jobject> j_video_capabilities_;
  const Range supported_widths_;
  const Range supported_heights_;
  const Range supported_bitrates_;
  const Range supported_frame_rates_;
};

class MediaCapabilitiesProvider {
 public:
  virtual ~MediaCapabilitiesProvider() = default;
  virtual bool GetIsWidevineSupported() = 0;
  virtual bool GetIsCbcsSchemeSupported() = 0;
  virtual std::set<SbMediaTransferId> GetSupportedHdrTypes() = 0;
  virtual bool GetIsPassthroughSupported(SbMediaAudioCodec codec) = 0;
  virtual bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* configuration) = 0;

  typedef std::vector<std::unique_ptr<AudioCodecCapability>>
      AudioCodecCapabilities;
  typedef std::vector<std::unique_ptr<VideoCodecCapability>>
      VideoCodecCapabilities;
  virtual void GetCodecCapabilities(
      std::map<std::string, AudioCodecCapabilities>& audio_codec_capabilities,
      std::map<std::string, VideoCodecCapabilities>&
          video_codec_capabilities) = 0;
};

class MediaCapabilitiesCache {
 public:
  static MediaCapabilitiesCache* GetInstance();

  ~MediaCapabilitiesCache() = default;
  bool IsWidevineSupported();
  bool IsCbcsSchemeSupported();

  bool IsHDRTransferCharacteristicsSupported(SbMediaTransferId transfer_id);

  bool IsPassthroughSupported(SbMediaAudioCodec codec);

  bool GetAudioConfiguration(int index,
                             SbMediaAudioConfiguration* configuration);

  bool HasAudioDecoderFor(const std::string& mime_type, int bitrate);

  bool HasVideoDecoderFor(const std::string& mime_type,
                          bool must_support_secure,
                          bool must_support_hdr,
                          bool must_support_tunnel_mode,
                          int frame_width,
                          int frame_height,
                          int bitrate,
                          int fps);

  std::string FindAudioDecoder(const std::string& mime_type, int bitrate);

  std::string FindVideoDecoder(const std::string& mime_type,
                               bool must_support_secure,
                               bool must_support_hdr,
                               bool require_software_codec,
                               bool must_support_tunnel_mode,
                               int frame_width,
                               int frame_height,
                               int bitrate,
                               int fps);

  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }
  void ClearCache() { capabilities_is_dirty_ = true; }

 protected:
  MediaCapabilitiesCache(
      std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider);

 private:
  MediaCapabilitiesCache();

  MediaCapabilitiesCache(const MediaCapabilitiesCache&) = delete;
  MediaCapabilitiesCache& operator=(const MediaCapabilitiesCache&) = delete;

  void UpdateMediaCapabilities_Locked();
  void LoadAudioConfigurations_Locked();

  std::mutex mutex_;

  // Provider for abstracting data sources. This must be non-null.
  const std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider_;

  // Cached data.
  std::set<SbMediaTransferId> supported_transfer_ids_;
  std::map<SbMediaAudioCodec, bool> passthrough_supportabilities_;

  typedef std::vector<std::unique_ptr<AudioCodecCapability>>
      AudioCodecCapabilities;
  typedef std::vector<std::unique_ptr<VideoCodecCapability>>
      VideoCodecCapabilities;
  std::map<std::string, AudioCodecCapabilities> audio_codec_capabilities_map_;
  std::map<std::string, VideoCodecCapabilities> video_codec_capabilities_map_;
  std::vector<SbMediaAudioConfiguration> audio_configurations_;
  bool is_widevine_supported_ = false;
  bool is_cbcs_supported_ = false;

  std::atomic_bool is_enabled_{true};
  std::atomic_bool capabilities_is_dirty_{true};
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_
