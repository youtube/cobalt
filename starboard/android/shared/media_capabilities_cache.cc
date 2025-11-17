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

#include <utility>

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/thread.h"

namespace starboard::android::shared {
namespace {

using base::android::AttachCurrentThread;
using ::starboard::shared::starboard::media::KeySystemSupportabilityCache;
using ::starboard::shared::starboard::media::MimeSupportabilityCache;

// https://developer.android.com/reference/android/view/Display.HdrCapabilities.html#HDR_TYPE_HDR10
const jint HDR_TYPE_DOLBY_VISION = 1;
const jint HDR_TYPE_HDR10 = 2;
const jint HDR_TYPE_HLG = 3;
const jint HDR_TYPE_HDR10_PLUS = 4;

const char SECURE_DECODER_SUFFIX[] = ".secure";

// Constants for output types from
// https://developer.android.com/reference/android/media/AudioDeviceInfo.
constexpr int TYPE_AUX_LINE = 19;
constexpr int TYPE_BLE_BROADCAST = 30;
constexpr int TYPE_BLE_HEADSET = 26;
constexpr int TYPE_BLE_SPEAKER = 27;
constexpr int TYPE_BLUETOOTH_A2DP = 8;
constexpr int TYPE_BLUETOOTH_SCO = 7;
constexpr int TYPE_BUILTIN_EARPIECE = 1;
constexpr int TYPE_BUILTIN_MIC = 15;
constexpr int TYPE_BUILTIN_SPEAKER = 2;
constexpr int TYPE_BUILTIN_SPEAKER_SAFE = 24;
constexpr int TYPE_BUS = 21;
constexpr int TYPE_DOCK = 13;
constexpr int TYPE_DOCK_ANALOG = 31;
constexpr int TYPE_FM = 14;
constexpr int TYPE_FM_TUNER = 16;
constexpr int TYPE_HDMI = 9;
constexpr int TYPE_HDMI_ARC = 10;
constexpr int TYPE_HDMI_EARC = 29;
constexpr int TYPE_HEARING_AID = 23;
constexpr int TYPE_IP = 20;
constexpr int TYPE_LINE_ANALOG = 5;
constexpr int TYPE_LINE_DIGITAL = 6;
constexpr int TYPE_REMOTE_SUBMIX = 25;
constexpr int TYPE_TELEPHONY = 18;
constexpr int TYPE_TV_TUNER = 17;
constexpr int TYPE_UNKNOWN = 0;
constexpr int TYPE_USB_ACCESSORY = 12;
constexpr int TYPE_USB_DEVICE = 11;
constexpr int TYPE_USB_HEADSET = 22;
constexpr int TYPE_WIRED_HEADPHONES = 4;
constexpr int TYPE_WIRED_HEADSET = 3;

SbMediaAudioConnector GetConnectorFromAndroidOutputType(
    int android_output_device_type) {
  switch (android_output_device_type) {
    case TYPE_AUX_LINE:
      return kSbMediaAudioConnectorAnalog;
    case TYPE_BLE_BROADCAST:
      return kSbMediaAudioConnectorBluetooth;
    case TYPE_BLE_HEADSET:
      return kSbMediaAudioConnectorBluetooth;
    case TYPE_BLE_SPEAKER:
      return kSbMediaAudioConnectorBluetooth;
    case TYPE_BLUETOOTH_A2DP:
      return kSbMediaAudioConnectorBluetooth;
    case TYPE_BLUETOOTH_SCO:
      return kSbMediaAudioConnectorBluetooth;
    case TYPE_BUILTIN_EARPIECE:
      return kSbMediaAudioConnectorBuiltIn;
    case TYPE_BUILTIN_MIC:
      return kSbMediaAudioConnectorBuiltIn;
    case TYPE_BUILTIN_SPEAKER:
      return kSbMediaAudioConnectorBuiltIn;
    case TYPE_BUILTIN_SPEAKER_SAFE:
      return kSbMediaAudioConnectorBuiltIn;
    case TYPE_BUS:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_DOCK:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_DOCK_ANALOG:
      return kSbMediaAudioConnectorAnalog;
    case TYPE_FM:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_FM_TUNER:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_HDMI:
      return kSbMediaAudioConnectorHdmi;
    case TYPE_HDMI_ARC:
      return kSbMediaAudioConnectorHdmi;
    case TYPE_HDMI_EARC:
      return kSbMediaAudioConnectorHdmi;
    case TYPE_HEARING_AID:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_IP:
      return kSbMediaAudioConnectorRemoteWired;
    case TYPE_LINE_ANALOG:
      return kSbMediaAudioConnectorAnalog;
    case TYPE_LINE_DIGITAL:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_REMOTE_SUBMIX:
      return kSbMediaAudioConnectorRemoteOther;
    case TYPE_TELEPHONY:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_TV_TUNER:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_UNKNOWN:
      return kSbMediaAudioConnectorUnknown;
    case TYPE_USB_ACCESSORY:
      return kSbMediaAudioConnectorUsb;
    case TYPE_USB_DEVICE:
      return kSbMediaAudioConnectorUsb;
    case TYPE_USB_HEADSET:
      return kSbMediaAudioConnectorUsb;
    case TYPE_WIRED_HEADPHONES:
      return kSbMediaAudioConnectorAnalog;
    case TYPE_WIRED_HEADSET:
      return kSbMediaAudioConnectorAnalog;
  }

  SB_LOG(WARNING) << "Encountered unknown audio output device type "
                  << android_output_device_type;
  return kSbMediaAudioConnectorUnknown;
}

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
  return MediaDrmBridge::IsWidevineSupported(AttachCurrentThread());
}

bool GetIsCbcsSupported() {
  return MediaDrmBridge::IsCbcsSupported(AttachCurrentThread());
}

std::set<SbMediaTransferId> GetSupportedHdrTypes() {
  std::set<SbMediaTransferId> supported_transfer_ids;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jintArray> j_supported_hdr_types =
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

bool GetAudioConfiguration(int index,
                           SbMediaAudioConfiguration* configuration) {
  *configuration = {};

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  ScopedLocalJavaRef<jobject> j_output_device_info(env->NewObjectOrAbort(
      "dev/cobalt/media/AudioOutputManager$OutputDeviceInfo", "()V"));

  bool succeeded = env->CallBooleanMethodOrAbort(
      j_audio_output_manager.Get(), "getOutputDeviceInfo",
      "(ILdev/cobalt/media/AudioOutputManager$OutputDeviceInfo;)Z", index,
      j_output_device_info.Get());

  if (!succeeded) {
    SB_LOG(WARNING)
        << "Call to AudioOutputManager.getOutputDeviceInfo() failed.";
    return false;
  }

  auto call_int_method = [env, &j_output_device_info](const char* name) {
    return env->CallIntMethodOrAbort(j_output_device_info.Get(), name, "()I");
  };

  configuration->connector =
      GetConnectorFromAndroidOutputType(call_int_method("getType"));
  configuration->latency = 0;
  configuration->coding_type = kSbMediaAudioCodingTypePcm;
  configuration->number_of_channels = call_int_method("getChannels");

  if (configuration->connector != kSbMediaAudioConnectorHdmi) {
    configuration->number_of_channels = 2;
  }

  return true;
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

bool VideoCodecCapability::AreResolutionAndRateSupported(int frame_width,
                                                         int frame_height,
                                                         int fps) {
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
  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  return is_widevine_supported_;
}

bool MediaCapabilitiesCache::IsCbcsSchemeSupported() {
  if (!is_enabled_) {
    return GetIsCbcsSupported();
  }
  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  return is_cbcs_supported_;
}

bool MediaCapabilitiesCache::IsHDRTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id) {
  if (!is_enabled_) {
    std::set<SbMediaTransferId> supported_transfer_ids = GetSupportedHdrTypes();
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
    return GetIsPassthroughSupported(codec);
  }
  // IsPassthroughSupported() caches the results of previous quiries, and does
  // not rely on LazyInitialize(), which is different from other functions.
  std::lock_guard scoped_lock(mutex_);
  auto iter = passthrough_supportabilities_.find(codec);
  if (iter != passthrough_supportabilities_.end()) {
    return iter->second;
  }
  bool supported = GetIsPassthroughSupported(codec);
  passthrough_supportabilities_[codec] = supported;
  return supported;
}

bool MediaCapabilitiesCache::GetAudioConfiguration(
    int index,
    SbMediaAudioConfiguration* configuration) {
  SB_DCHECK(index >= 0);
  if (!is_enabled_) {
    return ::starboard::android::shared::GetAudioConfiguration(index,
                                                               configuration);
  }

  std::lock_guard scoped_lock(mutex_);
  UpdateMediaCapabilities_Locked();
  if (index < audio_configurations_.size()) {
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
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(
        env->NewStringStandardUTFOrAbort(mime_type.c_str()));
    ScopedLocalJavaRef<jstring> j_decoder_name(
        static_cast<jstring>(env->CallStaticObjectMethodOrAbort(
            "dev/cobalt/media/MediaCodecUtil", "findAudioDecoder",
            "(Ljava/lang/String;I)Ljava/lang/String;", j_mime.Get(), bitrate)));
    return env->GetStringStandardUTFOrAbort(j_decoder_name.Get());
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
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(
        env->NewStringStandardUTFOrAbort(mime_type.c_str()));
    ScopedLocalJavaRef<jstring> j_decoder_name(
        static_cast<jstring>(env->CallStaticObjectMethodOrAbort(
            "dev/cobalt/media/MediaCodecUtil", "findVideoDecoder",
            "(Ljava/lang/String;ZZZZIIIII)Ljava/lang/String;", j_mime.Get(),
            must_support_secure, must_support_hdr,
            false,                        /* mustSupportSoftwareCodec */
            must_support_tunnel_mode, -1, /* decoderCacheTtlMs */
            frame_width, frame_height, bitrate, fps)));
    return env->GetStringStandardUTFOrAbort(j_decoder_name.Get());
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

MediaCapabilitiesCache::MediaCapabilitiesCache() {
  // Enable mime and key system caches.
  MimeSupportabilityCache::GetInstance()->SetCacheEnabled(true);
  KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(true);
}

void MediaCapabilitiesCache::UpdateMediaCapabilities_Locked() {
  if (capabilities_is_dirty_.exchange(false)) {
    // We use a different cache strategy (load and cache) for passthrough
    // supportabilities, so we only clear |passthrough_supportabilities_| here.
    passthrough_supportabilities_.clear();

    audio_codec_capabilities_map_.clear();
    video_codec_capabilities_map_.clear();
    audio_configurations_.clear();
    is_widevine_supported_ = GetIsWidevineSupported();
    is_cbcs_supported_ = GetIsCbcsSupported();
    supported_transfer_ids_ = GetSupportedHdrTypes();
    LoadCodecInfos_Locked();
    LoadAudioConfigurations_Locked();
  }
}

void MediaCapabilitiesCache::LoadCodecInfos_Locked() {
  SB_DCHECK(audio_codec_capabilities_map_.empty());
  SB_DCHECK(video_codec_capabilities_map_.empty());

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

void MediaCapabilitiesCache::LoadAudioConfigurations_Locked() {
  SB_DCHECK(audio_configurations_.empty());

  // SbPlayerBridge::GetAudioConfigurations() reads up to 32 configurations. The
  // limit here is to avoid infinite loop and also match
  // SbPlayerBridge::GetAudioConfigurations().
  const int kMaxAudioConfigurations = 32;
  SbMediaAudioConfiguration configuration;
  while (audio_configurations_.size() < kMaxAudioConfigurations &&
         ::starboard::android::shared::GetAudioConfiguration(
             audio_configurations_.size(), &configuration)) {
    audio_configurations_.push_back(configuration);
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_util_DisplayUtil_nativeOnDisplayChanged() {
  // Display device change could change hdr capabilities.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_AudioOutputManager_nativeOnAudioDeviceChanged() {
  // Audio output device change could change passthrough decoder capabilities,
  // so we have to reload codec capabilities.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

}  // namespace starboard::android::shared
