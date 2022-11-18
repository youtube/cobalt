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

#include <utility>

#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::starboard::shared::starboard::media::KeySystemSupportabilityCache;
using ::starboard::shared::starboard::media::MimeSupportabilityCache;

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

Range ConvertJavaRangeToRange(JniEnvExt* env, jobject j_range) {
  ScopedLocalJavaRef<jobject> j_upper_comparable(env->CallObjectMethodOrAbort(
      j_range, "getUpper", "()Ljava/lang/Comparable;"));
  jint j_upper_int =
      env->CallIntMethodOrAbort(j_upper_comparable.Get(), "intValue", "()I");

  ScopedLocalJavaRef<jobject> j_lower_comparable(env->CallObjectMethodOrAbort(
      j_range, "getLower", "()Ljava/lang/Comparable;"));
  jint j_lower_int =
      env->CallIntMethodOrAbort(j_lower_comparable.Get(), "intValue", "()I");
  return Range(j_lower_int, j_upper_int);
}

void ConvertStringToLowerCase(std::string* str) {
  for (int i = 0; i < str->length(); i++) {
    (*str)[i] = std::tolower((*str)[i]);
  }
}

bool GetIsWidevineSupported() {
  return JniEnvExt::Get()->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaDrmBridge",
             "isWidevineCryptoSchemeSupported", "()Z") == JNI_TRUE;
}

bool GetIsCbcsSupported() {
  return JniEnvExt::Get()->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaDrmBridge", "isCbcsSchemeSupported",
             "()Z") == JNI_TRUE;
}

std::set<SbMediaTransferId> GetSupportedHdrTypes() {
  std::set<SbMediaTransferId> supported_transfer_ids;

  JniEnvExt* env = JniEnvExt::Get();
  jintArray j_supported_hdr_types = static_cast<jintArray>(
      env->CallStarboardObjectMethodOrAbort("getSupportedHdrTypes", "()[I"));

  if (!j_supported_hdr_types) {
    // Failed to get supported hdr types.
    SB_LOG(ERROR) << "Failed to load supported hdr types.";
    return std::set<SbMediaTransferId>();
  }

  jsize length = env->GetArrayLength(j_supported_hdr_types);
  jint* numbers = env->GetIntArrayElements(j_supported_hdr_types, 0);
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
  env->ReleaseIntArrayElements(j_supported_hdr_types, numbers, 0);

  return supported_transfer_ids;
}

bool GetIsPassthroughSupported(SbMediaAudioCodec codec) {
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
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  return env->CallBooleanMethodOrAbort(j_audio_output_manager.Get(),
                                       "hasPassthroughSupportFor", "(I)Z",
                                       encoding) == JNI_TRUE;
}

int GetMaxAudioOutputChannels() {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  return static_cast<int>(env->CallIntMethodOrAbort(
      j_audio_output_manager.Get(), "getMaxChannels", "()I"));
}

}  // namespace

CodecCapability::CodecCapability(JniEnvExt* env, jobject j_codec_info) {
  SB_DCHECK(env);
  SB_DCHECK(j_codec_info);

  ScopedLocalJavaRef<jstring> j_decoder_name(
      env->GetStringFieldOrAbort(j_codec_info, "decoderName"));
  name_ = env->GetStringStandardUTFOrAbort(j_decoder_name.Get());
  is_secure_required_ =
      env->CallBooleanMethodOrAbort(j_codec_info, "isSecureRequired", "()Z") ==
      JNI_TRUE;
  is_secure_supported_ =
      env->CallBooleanMethodOrAbort(j_codec_info, "isSecureSupported", "()Z") ==
      JNI_TRUE;
  is_tunnel_mode_required_ =
      env->CallBooleanMethodOrAbort(j_codec_info, "isTunnelModeRequired",
                                    "()Z") == JNI_TRUE;
  is_tunnel_mode_supported_ =
      env->CallBooleanMethodOrAbort(j_codec_info, "isTunnelModeSupported",
                                    "()Z") == JNI_TRUE;
}

AudioCodecCapability::AudioCodecCapability(JniEnvExt* env,
                                           jobject j_codec_info,
                                           jobject j_audio_capabilities)
    : CodecCapability(env, j_codec_info) {
  SB_DCHECK(env);
  SB_DCHECK(j_codec_info);
  SB_DCHECK(j_audio_capabilities);

  ScopedLocalJavaRef<jobject> j_bitrate_range(env->CallObjectMethodOrAbort(
      j_audio_capabilities, "getBitrateRange", "()Landroid/util/Range;"));
  supported_bitrates_ = ConvertJavaRangeToRange(env, j_bitrate_range.Get());

  // Overwrite the lower bound to 0.
  supported_bitrates_.minimum = 0;
}

bool AudioCodecCapability::IsBitrateSupported(int bitrate) const {
  return supported_bitrates_.Contains(bitrate);
}

VideoCodecCapability::VideoCodecCapability(JniEnvExt* env,
                                           jobject j_codec_info,
                                           jobject j_video_capabilities)
    : CodecCapability(env, j_codec_info) {
  SB_DCHECK(env);
  SB_DCHECK(j_codec_info);
  SB_DCHECK(j_video_capabilities);

  is_software_decoder_ = env->CallBooleanMethodOrAbort(
                             j_codec_info, "isSoftware", "()Z") == JNI_TRUE;

  is_hdr_capable_ = env->CallBooleanMethodOrAbort(j_codec_info, "isHdrCapable",
                                                  "()Z") == JNI_TRUE;

  j_video_capabilities_ = env->NewGlobalRef(j_video_capabilities);

  ScopedLocalJavaRef<jobject> j_width_range(env->CallObjectMethodOrAbort(
      j_video_capabilities_, "getSupportedWidths", "()Landroid/util/Range;"));
  supported_widths_ = ConvertJavaRangeToRange(env, j_width_range.Get());

  ScopedLocalJavaRef<jobject> j_height_range(env->CallObjectMethodOrAbort(
      j_video_capabilities_, "getSupportedHeights", "()Landroid/util/Range;"));
  supported_heights_ = ConvertJavaRangeToRange(env, j_height_range.Get());

  ScopedLocalJavaRef<jobject> j_bitrate_range(env->CallObjectMethodOrAbort(
      j_video_capabilities_, "getBitrateRange", "()Landroid/util/Range;"));
  supported_bitrates_ = ConvertJavaRangeToRange(env, j_bitrate_range.Get());

  ScopedLocalJavaRef<jobject> j_frame_rate_range(env->CallObjectMethodOrAbort(
      j_video_capabilities_, "getSupportedFrameRates",
      "()Landroid/util/Range;"));
  supported_frame_rates_ =
      ConvertJavaRangeToRange(env, j_frame_rate_range.Get());
}

VideoCodecCapability::~VideoCodecCapability() {
  JniEnvExt* env = JniEnvExt::Get();
  env->DeleteGlobalRef(j_video_capabilities_);
}

bool VideoCodecCapability::IsBitrateSupported(int bitrate) const {
  return supported_bitrates_.Contains(bitrate);
}

bool VideoCodecCapability::AreResolutionAndRateSupported(
    bool force_improved_support_check,
    int frame_width,
    int frame_height,
    int fps) {
  if (force_improved_support_check) {
    if (frame_width != 0 && frame_height != 0 && fps != 0) {
      return JniEnvExt::Get()->CallBooleanMethodOrAbort(
                 j_video_capabilities_, "areSizeAndRateSupported", "(IID)Z",
                 frame_width, frame_height,
                 static_cast<jdouble>(fps)) == JNI_TRUE;
    } else if (frame_width != 0 && frame_height != 0) {
      return JniEnvExt::Get()->CallBooleanMethodOrAbort(
                 j_video_capabilities_, "isSizeSupported", "(II)Z", frame_width,
                 frame_height) == JNI_TRUE;
    }
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
                            MediaCapabilitiesCache::GetInstance);

bool MediaCapabilitiesCache::IsWidevineSupported() {
  if (!is_enabled_) {
    return GetIsWidevineSupported();
  }
  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();
  return is_widevine_supported_;
}

bool MediaCapabilitiesCache::IsCbcsSchemeSupported() {
  if (!is_enabled_) {
    return GetIsCbcsSupported();
  }
  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();
  return is_cbcs_supported_;
}

bool MediaCapabilitiesCache::IsHDRTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id) {
  if (!is_enabled_) {
    std::set<SbMediaTransferId> supported_transfer_ids = GetSupportedHdrTypes();
    return supported_transfer_ids.find(transfer_id) !=
           supported_transfer_ids.end();
  }
  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();
  return supported_transfer_ids_.find(transfer_id) !=
         supported_transfer_ids_.end();
}

bool MediaCapabilitiesCache::IsPassthroughSupported(SbMediaAudioCodec codec) {
  if (!is_enabled_) {
    return GetIsPassthroughSupported(codec);
  }
  // IsPassthroughSupported() caches the results of previous quiries, and does
  // not rely on LazyInitialize(), which is different from other functions.
  ScopedLock scoped_lock(mutex_);
  auto iter = passthrough_supportabilities_.find(codec);
  if (iter != passthrough_supportabilities_.end()) {
    return iter->second;
  }
  bool supported = GetIsPassthroughSupported(codec);
  passthrough_supportabilities_[codec] = supported;
  return supported;
}

int MediaCapabilitiesCache::GetMaxAudioOutputChannels() {
  if (!is_enabled_) {
    return ::starboard::android::shared::GetMaxAudioOutputChannels();
  }

  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();
  return max_audio_output_channels_;
}

bool MediaCapabilitiesCache::HasAudioDecoderFor(const std::string& mime_type,
                                                int bitrate,
                                                bool must_support_tunnel_mode) {
  return !FindAudioDecoder(mime_type, bitrate, must_support_tunnel_mode)
              .empty();
}

bool MediaCapabilitiesCache::HasVideoDecoderFor(
    const std::string& mime_type,
    bool must_support_secure,
    bool must_support_hdr,
    bool must_support_tunnel_mode,
    bool force_improved_support_check,
    int frame_width,
    int frame_height,
    int bitrate,
    int fps) {
  return !FindVideoDecoder(mime_type, must_support_secure, must_support_hdr,
                           false, must_support_tunnel_mode,
                           force_improved_support_check, frame_width,
                           frame_height, bitrate, fps)
              .empty();
}

std::string MediaCapabilitiesCache::FindAudioDecoder(
    const std::string& mime_type,
    int bitrate,
    bool must_support_tunnel_mode) {
  if (!is_enabled_) {
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(
        env->NewStringStandardUTFOrAbort(mime_type.c_str()));
    jobject j_decoder_name = env->CallStaticObjectMethodOrAbort(
        "dev/cobalt/media/MediaCodecUtil", "findAudioDecoder",
        "(Ljava/lang/String;IZ)Ljava/lang/String;", j_mime.Get(), bitrate,
        must_support_tunnel_mode);
    return env->GetStringStandardUTFOrAbort(
        static_cast<jstring>(j_decoder_name));
  }

  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();

  for (auto& audio_capability : audio_codec_capabilities_map_[mime_type]) {
    // Reject if tunnel mode is required but codec doesn't support it.
    if (must_support_tunnel_mode &&
        !audio_capability->is_tunnel_mode_supported()) {
      continue;
    }
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
    bool force_improved_support_check,
    int frame_width,
    int frame_height,
    int bitrate,
    int fps) {
  if (!is_enabled_) {
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(
        env->NewStringStandardUTFOrAbort(mime_type.c_str()));
    jobject j_decoder_name = env->CallStaticObjectMethodOrAbort(
        "dev/cobalt/media/MediaCodecUtil", "findVideoDecoder",
        "(Ljava/lang/String;ZZZZZIIIII)Ljava/lang/String;", j_mime.Get(),
        must_support_secure, must_support_hdr,
        false, /* mustSupportSoftwareCodec */
        must_support_tunnel_mode, force_improved_support_check,
        -1, /* decoderCacheTtlMs */
        frame_width, frame_height, bitrate, fps);
    return env->GetStringStandardUTFOrAbort(
        static_cast<jstring>(j_decoder_name));
  }

  ScopedLock scoped_lock(mutex_);
  LazyInitialize_Locked();

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

    bool enable_improved_support_check =
        force_improved_support_check ||
        (frame_width > 3840 || frame_height > 2160);
    // Reject if resolution or frame rate is not supported.
    if (!video_capability->AreResolutionAndRateSupported(
            enable_improved_support_check, frame_width, frame_height, fps)) {
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

void MediaCapabilitiesCache::ClearCache() {
  ScopedLock scoped_lock(mutex_);
  is_initialized_ = false;
  is_widevine_supported_ = false;
  is_cbcs_supported_ = false;
  supported_transfer_ids_.clear();
  passthrough_supportabilities_.clear();
  audio_codec_capabilities_map_.clear();
  video_codec_capabilities_map_.clear();
  max_audio_output_channels_ = -1;
}

void MediaCapabilitiesCache::ReloadSupportedHdrTypes() {
  ScopedLock scoped_lock(mutex_);
  if (!is_initialized_) {
    LazyInitialize_Locked();
    return;
  }
  supported_transfer_ids_ = GetSupportedHdrTypes();
}

void MediaCapabilitiesCache::ReloadAudioOutputChannels() {
  ScopedLock scoped_lock(mutex_);
  if (!is_initialized_) {
    LazyInitialize_Locked();
    return;
  }
  max_audio_output_channels_ =
      ::starboard::android::shared::GetMaxAudioOutputChannels();
}

MediaCapabilitiesCache::MediaCapabilitiesCache() {
  // Enable mime and key system caches.
  MimeSupportabilityCache::GetInstance()->SetCacheEnabled(true);
  KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(true);
}

void MediaCapabilitiesCache::LazyInitialize_Locked() {
  mutex_.DCheckAcquired();

  if (is_initialized_) {
    return;
  }
  is_widevine_supported_ = GetIsWidevineSupported();
  is_cbcs_supported_ = GetIsCbcsSupported();
  supported_transfer_ids_ = GetSupportedHdrTypes();
  max_audio_output_channels_ =
      ::starboard::android::shared::GetMaxAudioOutputChannels();

  LoadCodecInfos_Locked();

  is_initialized_ = true;
}

void MediaCapabilitiesCache::LoadCodecInfos_Locked() {
  SB_DCHECK(audio_codec_capabilities_map_.empty());
  SB_DCHECK(video_codec_capabilities_map_.empty());
  mutex_.DCheckAcquired();

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobjectArray> j_codec_infos(
      static_cast<jobjectArray>(env->CallStaticObjectMethodOrAbort(
          "dev/cobalt/media/MediaCodecUtil", "getAllCodecCapabilityInfos",
          "()[Ldev/cobalt/media/MediaCodecUtil$CodecCapabilityInfo;")));
  jsize length = env->GetArrayLength(j_codec_infos.Get());
  // Note: Codec infos are sorted by the framework such that the best
  // decoders come first.
  // This order is maintained in the cache.
  for (int i = 0; i < length; i++) {
    ScopedLocalJavaRef<jobject> j_codec_info(
        env->GetObjectArrayElementOrAbort(j_codec_infos.Get(), i));

    ScopedLocalJavaRef<jstring> j_mime_type(
        env->GetStringFieldOrAbort(j_codec_info.Get(), "mimeType"));
    std::string mime_type = env->GetStringStandardUTFOrAbort(j_mime_type.Get());
    // Convert the mime type to lower case.
    ConvertStringToLowerCase(&mime_type);

    ScopedLocalJavaRef<jobject> j_audio_capabilities(env->GetObjectFieldOrAbort(
        j_codec_info.Get(), "audioCapabilities",
        "Landroid/media/MediaCodecInfo$AudioCapabilities;"));
    if (j_audio_capabilities) {
      // Found an audio decoder.
      std::unique_ptr<AudioCodecCapability> audio_codec_capabilities(
          new AudioCodecCapability(env, j_codec_info.Get(),
                                   j_audio_capabilities.Get()));
      audio_codec_capabilities_map_[mime_type].push_back(
          std::move(audio_codec_capabilities));
      continue;
    }
    ScopedLocalJavaRef<jobject> j_video_capabilities(env->GetObjectFieldOrAbort(
        j_codec_info.Get(), "videoCapabilities",
        "Landroid/media/MediaCodecInfo$VideoCapabilities;"));
    if (j_video_capabilities) {
      // Found a video decoder.
      std::unique_ptr<VideoCodecCapability> video_codec_capabilities(
          new VideoCodecCapability(env, j_codec_info.Get(),
                                   j_video_capabilities.Get()));
      video_codec_capabilities_map_[mime_type].push_back(
          std::move(video_codec_capabilities));
    }
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_util_DisplayUtil_nativeOnDisplayChanged() {
  SB_DLOG(INFO) << "Display device has changed.";
  MediaCapabilitiesCache::GetInstance()->ReloadSupportedHdrTypes();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_AudioOutputManager_nativeOnAudioDeviceChanged() {
  SB_DLOG(INFO) << "Audio device has changed.";
  MediaCapabilitiesCache::GetInstance()->ReloadAudioOutputChannels();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
