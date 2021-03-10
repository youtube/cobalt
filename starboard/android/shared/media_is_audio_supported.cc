// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/mime_type.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;
using starboard::android::shared::SupportedAudioCodecToMimeType;
using starboard::shared::starboard::media::MimeType;

bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                             const char* content_type,
                             int64_t bitrate) {
  // Android now uses libopus based opus decoder.
  if (audio_codec == kSbMediaAudioCodecOpus &&
      bitrate < kSbMediaMaxAudioBitrateInBitsPerSecond) {
    return true;
  }
  const char* mime = SupportedAudioCodecToMimeType(audio_codec);
  if (!mime) {
    return false;
  }
  MimeType mime_type(content_type);
  // Allows for disabling the use of the AudioRouting API to detect when audio
  // peripherals are connected. Enabled by default.
  // (https://developer.android.com/reference/android/media/AudioRouting)
  auto enable_audio_routing_parameter_value =
      mime_type.GetParamStringValue("enableaudiorouting", "");
  if (!enable_audio_routing_parameter_value.empty() &&
      enable_audio_routing_parameter_value != "true" &&
      enable_audio_routing_parameter_value != "false") {
    SB_LOG(INFO)
        << "Invalid value for audio mime parameter \"enableaudiorouting\": "
        << enable_audio_routing_parameter_value << ".";
    return false;
  }
  // Allows for enabling tunneled playback. Disabled by default.
  // (https://source.android.com/devices/tv/multimedia-tunneling)
  auto enable_tunnel_mode_parameter_value =
      mime_type.GetParamStringValue("tunnelmode", "");
  if (!enable_tunnel_mode_parameter_value.empty() &&
      enable_tunnel_mode_parameter_value != "true" &&
      enable_tunnel_mode_parameter_value != "false") {
    SB_LOG(INFO) << "Invalid value for audio mime parameter \"tunnelmode\": "
                 << enable_tunnel_mode_parameter_value << ".";
    return false;
  } else if (enable_tunnel_mode_parameter_value == "true" &&
             !SbAudioSinkIsAudioSampleTypeSupported(
                 kSbMediaAudioSampleTypeInt16Deprecated)) {
    SB_LOG(WARNING)
        << "Tunnel mode is rejected because int16 sample is required "
           "but not supported.";
    return false;
  }

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
  const bool must_support_tunnel_mode =
      enable_tunnel_mode_parameter_value == "true";
  return env->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaCodecUtil", "hasAudioDecoderFor",
             "(Ljava/lang/String;IZ)Z", j_mime.Get(),
             static_cast<jint>(bitrate), must_support_tunnel_mode) == JNI_TRUE;
}
