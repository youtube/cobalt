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

#include "starboard/media.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/common/media.h"

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

// TODO(b/284140486): Refine the implementation so it works when the audio
// outputs are changed during the query.
bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  using starboard::GetMediaAudioConnectorName;
  using starboard::android::shared::JniEnvExt;
  using starboard::android::shared::MediaCapabilitiesCache;
  using starboard::android::shared::ScopedLocalJavaRef;

  if (output_index < 0) {
    SB_LOG(WARNING) << "output_index is " << output_index
                    << ", which cannot be negative.";
    return false;
  }

  if (out_configuration == nullptr) {
    SB_LOG(WARNING) << "out_configuration cannot be nullptr.";
    return false;
  }

  *out_configuration = {};

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  ScopedLocalJavaRef<jobject> j_output_device_info(env->NewObjectOrAbort(
      "dev/cobalt/media/AudioOutputManager$OutputDeviceInfo", "()V"));

  bool succeeded = env->CallBooleanMethodOrAbort(
      j_audio_output_manager.Get(), "getOutputDeviceInfo",
      "(ILdev/cobalt/media/AudioOutputManager$OutputDeviceInfo;)Z",
      output_index, j_output_device_info.Get());

  if (!succeeded) {
    SB_LOG(WARNING)
        << "Call to AudioOutputManager.getOutputDeviceInfo() failed.";
    return false;
  }

  auto call_int_method = [env, &j_output_device_info](const char* name) {
    return env->CallIntMethodOrAbort(j_output_device_info.Get(), name, "()I");
  };

  out_configuration->connector =
      GetConnectorFromAndroidOutputType(call_int_method("getType"));
  out_configuration->latency = 0;
  out_configuration->coding_type = kSbMediaAudioCodingTypePcm;
  out_configuration->number_of_channels = call_int_method("getChannels");

  if (out_configuration->connector == kSbMediaAudioConnectorHdmi) {
    // Keep the previous logic for HDMI to reduce risk.
    // TODO(b/284140486): Update this using same logic as other connectors.
    int channels =
        MediaCapabilitiesCache::GetInstance()->GetMaxAudioOutputChannels();
    if (channels < 2) {
      SB_LOG(WARNING) << "The supported channels from output device is "
                      << channels << ", set to 2 channels instead.";
      out_configuration->number_of_channels = 2;
    } else {
      out_configuration->number_of_channels = channels;
    }
  } else {
    out_configuration->number_of_channels = 2;
  }

  SB_LOG(INFO) << "Audio connector type for index " << output_index << " is "
               << GetMediaAudioConnectorName(out_configuration->connector)
               << " and it has " << out_configuration->number_of_channels
               << " channels.";

  return true;
}
