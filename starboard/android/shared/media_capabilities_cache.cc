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

#include "starboard/android/shared/media_capabilities_cache.h"

#include <jni.h>

#include <mutex>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "cobalt/android/jni_headers/MediaCodecUtil_jni.h"
#include "starboard/android/shared/audio_output_manager.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/thread.h"

namespace starboard {
namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

// https://developer.android.com/reference/android/view/Display.HdrCapabilities.html#HDR_TYPE_HDR10
const jint HDR_TYPE_DOLBY_VISION = 1;
const jint HDR_TYPE_HDR10 = 2;
const jint HDR_TYPE_HLG = 3;
const jint HDR_TYPE_HDR10_PLUS = 4;

const char SECURE_DECODER_SUFFIX[] = ".secure";

bool EndsWith(const std::string& str, const std::string& suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }
  return strcmp(str.c_str() + (str.size() - suffix.size()), suffix.c_str()) ==
         0;
}

Range ConvertJavaRangeToRange(JNIEnv* env, jobject j_range) {
  const auto j_range_ref = JavaParamRef<jobject>(env, j_range);
  return Range(Java_MediaCodecUtil_getRangeLower(env, j_range_ref),
               Java_MediaCodecUtil_getRangeUpper(env, j_range_ref));
}

template <typename GetRangeFunc>
Range GetRange(JNIEnv* env,
               const base::android::JavaRef<jobject>& j_capabilities,
               GetRangeFunc get_range_func) {
  SB_CHECK(env);
  SB_CHECK(j_capabilities);
  ScopedJavaLocalRef<jobject> j_range = get_range_func(env, j_capabilities);
  SB_CHECK(j_range);
  return ConvertJavaRangeToRange(env, j_range.obj());
}

void ConvertStringToLowerCase(std::string* str) {
  for (size_t i = 0; i < str->length(); i++) {
    (*str)[i] = std::tolower((*str)[i]);
  }
}

class MediaCapabilitiesProviderImpl : public MediaCapabilitiesProvider {
  bool GetIsWidevineSupported() override {
    return MediaDrmBridge::IsWidevineSupported(AttachCurrentThread());
  }
  bool GetIsCbcsSchemeSupported() override {
    return MediaDrmBridge::IsCbcsSupported(AttachCurrentThread());
  }
  std::set<SbMediaTransferId> GetSupportedHdrTypes() override {
    std::set<SbMediaTransferId> supported_transfer_ids;

    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jintArray> j_supported_hdr_types =
        StarboardBridge::GetInstance()->GetSupportedHdrTypes(env);

    if (!j_supported_hdr_types) {
      // Failed to get supported hdr types.
      SB_LOG(ERROR) << "Failed to load supported hdr types.";
      return std::set<SbMediaTransferId>();
    }

    jsize length = env->GetArrayLength(j_supported_hdr_types.obj());
    jint* numbers =
        env->GetIntArrayElements(j_supported_hdr_types.obj(), nullptr);
    for (int i = 0; i < length; i++) {
      switch (numbers[i]) {
        case HDR_TYPE_DOLBY_VISION:
          continue;
        case HDR_TYPE_HDR10:
          supported_transfer_ids.insert(kSbMediaTransferIdSmpteSt2084);
          continue;
        case HDR_TYPE_HLG:
          supported_transfer_ids.insert(kSbMediaTransferIdAribStdB67);
          continue;
        case HDR_TYPE_HDR10_PLUS:
          continue;
      }
    }
    env->ReleaseIntArrayElements(j_supported_hdr_types.obj(), numbers, 0);

    return supported_transfer_ids;
  }
  bool GetIsPassthroughSupported(SbMediaAudioCodec codec) override {
    SbMediaAudioCodingType coding_type;
    switch (codec) {
      case kSbMediaAudioCodecAc3:
        coding_type = kSbMediaAudioCodingTypeAc3;
        break;
      case kSbMediaAudioCodecEac3:
        coding_type = kSbMediaAudioCodingTypeDolbyDigitalPlus;
        break;
      default:
        return false;
    }
    int encoding = GetAudioFormatSampleType(coding_type);
    JNIEnv* env = AttachCurrentThread();
    return AudioOutputManager::GetInstance()->HasPassthroughSupportFor(
        env, encoding);
  }
  bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* configuration) override {
    JNIEnv* env = AttachCurrentThread();
    return AudioOutputManager::GetInstance()->GetAudioConfiguration(
        env, index, configuration);
  }
  void GetCodecCapabilities(
      std::map<std::string, AudioCodecCapabilities>& audio_codec_capabilities,
      std::map<std::string, VideoCodecCapabilities>& video_codec_capabilities)
      override {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> j_codec_infos =
        Java_MediaCodecUtil_getAllCodecCapabilityInfos(env);
    jsize length = env->GetArrayLength(j_codec_infos.obj());

    // Note: Codec infos are sorted by the framework such that the best
    // decoders come first.
    // This order is maintained in the cache.
    for (int i = 0; i < length; i++) {
      ScopedJavaLocalRef<jobject> j_codec_info(
          env, env->GetObjectArrayElement(j_codec_infos.obj(), i));
      SB_CHECK(j_codec_info);

      ScopedJavaLocalRef<jstring> j_mime_type =
          Java_CodecCapabilityInfo_getMimeType(env, j_codec_info);
      std::string mime_type = ConvertJavaStringToUTF8(env, j_mime_type.obj());
      // Convert the mime type to lower case.
      ConvertStringToLowerCase(&mime_type);

      ScopedJavaLocalRef<jobject> j_audio_capabilities =
          Java_CodecCapabilityInfo_getAudioCapabilities(env, j_codec_info);
      if (j_audio_capabilities) {
        // Found an audio decoder.
        audio_codec_capabilities[mime_type].push_back(
            std::make_unique<AudioCodecCapability>(env, j_codec_info,
                                                   j_audio_capabilities));
        continue;
      }
      ScopedJavaLocalRef<jobject> j_video_capabilities =
          Java_CodecCapabilityInfo_getVideoCapabilities(env, j_codec_info);
      if (j_video_capabilities) {
        // Found a video decoder.
        video_codec_capabilities[mime_type].push_back(
            std::make_unique<VideoCodecCapability>(env, j_codec_info,
                                                   j_video_capabilities));
      }
    }
  }
};
}  // namespace

CodecCapability::CodecCapability(JNIEnv* env,
                                 ScopedJavaLocalRef<jobject>& j_codec_info)
    : name_(ConvertJavaStringToUTF8(
          env,
          Java_CodecCapabilityInfo_getDecoderName(env, j_codec_info))),
      is_secure_required_(
          Java_CodecCapabilityInfo_isSecureRequired(env, j_codec_info)),
      is_secure_supported_(
          Java_CodecCapabilityInfo_isSecureSupported(env, j_codec_info)),
      is_tunnel_mode_required_(
          Java_CodecCapabilityInfo_isTunnelModeRequired(env, j_codec_info)),
      is_tunnel_mode_supported_(
          Java_CodecCapabilityInfo_isTunnelModeSupported(env, j_codec_info)) {}

AudioCodecCapability::AudioCodecCapability(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& j_codec_info,
    ScopedJavaLocalRef<jobject>& j_audio_capabilities)
    : CodecCapability(env, j_codec_info),
      supported_bitrates_([env, &j_audio_capabilities] {
        Range supported_bitrates =
            GetRange(env, j_audio_capabilities,
                     &Java_MediaCodecUtil_getAudioBitrateRange);
        // Overwrite the lower bound to 0.
        supported_bitrates.minimum = 0;
        return supported_bitrates;
      }()) {
  SB_CHECK(j_codec_info);
}

bool AudioCodecCapability::IsBitrateSupported(int bitrate) const {
  return supported_bitrates_.Contains(bitrate);
}

VideoCodecCapability::VideoCodecCapability(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& j_codec_info,
    ScopedJavaLocalRef<jobject>& j_video_capabilities)
    : CodecCapability(env, j_codec_info),
      is_software_decoder_(
          Java_CodecCapabilityInfo_isSoftware(env, j_codec_info)),
      is_hdr_capable_(Java_CodecCapabilityInfo_isHdrCapable(env, j_codec_info)),
      j_video_capabilities_(env, j_video_capabilities.obj()),
      supported_widths_(GetRange(env,
                                 j_video_capabilities_,
                                 &Java_MediaCodecUtil_getVideoWidthRange)),
      supported_heights_(GetRange(env,
                                  j_video_capabilities_,
                                  &Java_MediaCodecUtil_getVideoHeightRange)),
      supported_bitrates_(GetRange(env,
                                   j_video_capabilities_,
                                   &Java_MediaCodecUtil_getVideoBitrateRange)),
      supported_frame_rates_(
          GetRange(env,
                   j_video_capabilities_,
                   &Java_MediaCodecUtil_getVideoFrameRateRange)) {}
VideoCodecCapability::~VideoCodecCapability() = default;

bool VideoCodecCapability::IsBitrateSupported(int bitrate) const {
  return supported_bitrates_.Contains(bitrate);
}

bool VideoCodecCapability::AreResolutionAndRateSupported(int frame_width,
                                                         int frame_height,
                                                         int fps) const {
  JNIEnv* env = AttachCurrentThread();
  if (frame_width != 0 && frame_height != 0 && fps != 0) {
    return Java_MediaCodecUtil_areSizeAndRateSupported(
        env, j_video_capabilities_, frame_width, frame_height,
        static_cast<jdouble>(fps));
  } else if (frame_width != 0 && frame_height != 0) {
    return Java_MediaCodecUtil_isSizeSupported(env, j_video_capabilities_,
                                               frame_width, frame_height);
  }
  if (frame_width != 0 && !supported_widths_.Contains(frame_width)) {
    return false;
  }
  if (frame_height != 0 && !supported_heights_.Contains(frame_height)) {
    return false;
  }
  if (fps != 0 && !supported_frame_rates_.Contains(fps)) {
    return false;
  }
  return true;
}

// static
SB_ONCE_INITIALIZE_FUNCTION(MediaCapabilitiesCache,
                            MediaCapabilitiesCache::GetInstance)

std::unique_ptr<MediaCapabilitiesCache> MediaCapabilitiesCache::CreateForTest(
    std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider) {
  return std::unique_ptr<MediaCapabilitiesCache>(
      new MediaCapabilitiesCache(std::move(media_capabilities_provider)));
}

MediaCapabilitiesCache::MediaCapabilitiesCache()
    : MediaCapabilitiesCache(
          std::make_unique<MediaCapabilitiesProviderImpl>()) {}

MediaCapabilitiesCache::MediaCapabilitiesCache(
    std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider)
    : media_capabilities_provider_(std::move(media_capabilities_provider)) {
  // Enable mime and key system caches.
  MimeSupportabilityCache::GetInstance()->SetCacheEnabled(true);
  KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(true);
}

bool MediaCapabilitiesCache::IsWidevineSupported() {
  if (!is_enabled_) {
    return media_capabilities_provider_->GetIsWidevineSupported();
  }
  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  return is_widevine_supported_;
}

bool MediaCapabilitiesCache::IsCbcsSchemeSupported() {
  if (!is_enabled_) {
    return media_capabilities_provider_->GetIsCbcsSchemeSupported();
  }
  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  return is_cbcs_supported_;
}

bool MediaCapabilitiesCache::IsHDRTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id) {
  if (!is_enabled_) {
    std::set<SbMediaTransferId> supported_transfer_ids =
        media_capabilities_provider_->GetSupportedHdrTypes();
    return supported_transfer_ids.find(transfer_id) !=
           supported_transfer_ids.end();
  }
  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  return supported_transfer_ids_.find(transfer_id) !=
         supported_transfer_ids_.end();
}

bool MediaCapabilitiesCache::IsPassthroughSupported(SbMediaAudioCodec codec) {
  if (!is_enabled_) {
    return media_capabilities_provider_->GetIsPassthroughSupported(codec);
  }
  // IsPassthroughSupported() caches the results of previous quiries, and does
  // not rely on LazyInitialize(), which is different from other functions.
  std::lock_guard scoped_lock(mutex_);
  auto iter = passthrough_supportabilities_.find(codec);
  if (iter != passthrough_supportabilities_.end()) {
    return iter->second;
  }
  bool supported =
      media_capabilities_provider_->GetIsPassthroughSupported(codec);
  passthrough_supportabilities_[codec] = supported;
  return supported;
}

bool MediaCapabilitiesCache::GetAudioConfiguration(
    int index,
    SbMediaAudioConfiguration* configuration) {
  SB_CHECK_GE(index, 0);
  if (!is_enabled_) {
    return media_capabilities_provider_->GetAudioConfiguration(index,
                                                               configuration);
  }

  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  if (static_cast<size_t>(index) < audio_configurations_.size()) {
    *configuration = audio_configurations_[index];
    return true;
  }
  return false;
}

bool MediaCapabilitiesCache::HasAudioDecoderFor(const std::string& mime_type,
                                                int bitrate) {
  return !FindAudioDecoder(mime_type, bitrate).empty();
}

bool MediaCapabilitiesCache::HasVideoDecoderFor(const std::string& mime_type,
                                                bool must_support_secure,
                                                bool must_support_hdr,
                                                bool must_support_tunnel_mode,
                                                int frame_width,
                                                int frame_height,
                                                int bitrate,
                                                int fps) {
  return !FindVideoDecoder(mime_type, must_support_secure, must_support_hdr,
                           false, must_support_tunnel_mode, frame_width,
                           frame_height, bitrate, fps)
              .empty();
}

std::string MediaCapabilitiesCache::FindAudioDecoder(
    const std::string& mime_type,
    int bitrate) {
  if (!is_enabled_) {
    JNIEnv* env = AttachCurrentThread();
    auto j_mime = ConvertUTF8ToJavaString(env, mime_type);
    auto j_decoder_name =
        Java_MediaCodecUtil_findAudioDecoder(env, j_mime, bitrate);
    return ConvertJavaStringToUTF8(env, j_decoder_name.obj());
  }

  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();

  for (auto& audio_capability : audio_codec_capabilities_map_[mime_type]) {
    // Reject if bitrate is not supported.
    if (!audio_capability->IsBitrateSupported(bitrate)) {
      continue;
    }
    return audio_capability->name();
  }

  return "";
}

std::string MediaCapabilitiesCache::FindVideoDecoder(
    const std::string& mime_type,
    bool must_support_secure,
    bool must_support_hdr,
    bool require_software_codec,
    bool must_support_tunnel_mode,
    int frame_width,
    int frame_height,
    int bitrate,
    int fps) {
  if (!is_enabled_) {
    JNIEnv* env = AttachCurrentThread();
    auto j_mime = ConvertUTF8ToJavaString(env, mime_type);
    auto j_decoder_name = Java_MediaCodecUtil_findVideoDecoder(
        env, j_mime, must_support_secure, must_support_hdr,
        /*mustSupportSoftwareCodec=*/false, must_support_tunnel_mode,
        /*decoderCacheTtlMs=*/-1, frame_width, frame_height, bitrate, fps);
    return ConvertJavaStringToUTF8(env, j_decoder_name.obj());
  }

  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();

  for (auto& video_capability : video_codec_capabilities_map_[mime_type]) {
    // Reject if secure decoder is required but codec doesn't support it.
    if (must_support_secure && !video_capability->is_secure_supported()) {
      continue;
    }
    // Reject if non secure decoder is required but codec doesn't support it.
    if (!must_support_secure && video_capability->is_secure_required()) {
      continue;
    }
    // Reject if tunnel mode is required but codec doesn't support it.
    if (must_support_tunnel_mode &&
        !video_capability->is_tunnel_mode_supported()) {
      continue;
    }
    // Reject if non tunnel mode is required but codec doesn't support it.
    if (!must_support_tunnel_mode &&
        video_capability->is_tunnel_mode_required()) {
      continue;
    }
    // Reject if software codec is required but codec is not.
    if (require_software_codec && !video_capability->is_software_decoder()) {
      continue;
    }
    // Reject if hdr is required but codec doesn't support it.
    if (must_support_hdr && !video_capability->is_hdr_capable()) {
      continue;
    }

    // Reject if resolution or frame rate is not supported.
    if (!video_capability->AreResolutionAndRateSupported(frame_width,
                                                         frame_height, fps)) {
      continue;
    }

    // Reject if bitrate is not supported.
    if (bitrate != 0 && !video_capability->IsBitrateSupported(bitrate)) {
      continue;
    }

    // Append ".secure" for secure decoder if not represents.
    if (must_support_secure &&
        !EndsWith(video_capability->name(), SECURE_DECODER_SUFFIX)) {
      return video_capability->name() + SECURE_DECODER_SUFFIX;
    }
    return video_capability->name();
  }

  return "";
}

void MediaCapabilitiesCache::UpdateMediaCapabilities_Locked() {
  if (!capabilities_is_dirty_.exchange(false)) {
    return;
  }
  // We use a different cache strategy (load and cache) for passthrough
  // supportabilities, so we only clear |passthrough_supportabilities_| here.
  passthrough_supportabilities_.clear();

  audio_codec_capabilities_map_.clear();
  video_codec_capabilities_map_.clear();
  audio_configurations_.clear();
  is_widevine_supported_ =
      media_capabilities_provider_->GetIsWidevineSupported();
  is_cbcs_supported_ = media_capabilities_provider_->GetIsCbcsSchemeSupported();
  supported_transfer_ids_ =
      media_capabilities_provider_->GetSupportedHdrTypes();
  media_capabilities_provider_->GetCodecCapabilities(
      audio_codec_capabilities_map_, video_codec_capabilities_map_);
  LoadAudioConfigurations_Locked();
}

void MediaCapabilitiesCache::LoadAudioConfigurations_Locked() {
  SB_CHECK(audio_configurations_.empty());

  // SbPlayerBridge::GetAudioConfigurations() reads up to 32 configurations.
  // The limit here is to avoid infinite loop and also match
  // SbPlayerBridge::GetAudioConfigurations().
  const int kMaxAudioConfigurations = 32;
  SbMediaAudioConfiguration configuration;
  while (audio_configurations_.size() < kMaxAudioConfigurations &&
         media_capabilities_provider_->GetAudioConfiguration(
             static_cast<int>(audio_configurations_.size()), &configuration)) {
    audio_configurations_.push_back(configuration);
  }
}

}  // namespace starboard
