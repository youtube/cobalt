// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_AUDIO_WRITE_AHEAD_H_
#define COBALT_EXTENSION_AUDIO_WRITE_AHEAD_H_

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionConfigurableAudioWriteAheadName \
  "dev.cobalt.extension.ConfigurableAudioWriteAhead"

#define kCobaltExtensionPlayerWriteDurationLocal (kSbTimeSecond / 2)
#define kCobaltExtensionPlayerWriteDurationRemote (kSbTimeSecond * 10)

typedef enum CobaltExtensionMediaAudioConnector {
  kCobaltExtensionMediaAudioConnectorUnknown,

  kCobaltExtensionMediaAudioConnectorAnalog,
  kCobaltExtensionMediaAudioConnectorBluetooth,
  kCobaltExtensionMediaAudioConnectorBuiltIn,
  kCobaltExtensionMediaAudioConnectorHdmi,
  kCobaltExtensionMediaAudioConnectorRemoteWired,
  kCobaltExtensionMediaAudioConnectorRemoteWireless,
  kCobaltExtensionMediaAudioConnectorRemoteOther,
  kCobaltExtensionMediaAudioConnectorSpdif,
  kCobaltExtensionMediaAudioConnectorUsb,
} CobaltExtensionMediaAudioConnector;

typedef struct CobaltExtensionMediaAudioConfiguration {
  CobaltExtensionMediaAudioConnector connector;
  SbTime latency;
  SbMediaAudioCodingType coding_type;
  int number_of_channels;
} CobaltExtensionMediaAudioConfiguration;

typedef struct CobaltExtensionConfigurableAudioWriteAheadApi {
  // Name should be the string
  // |kCobaltExtensionConfigurableAudioWriteAheadName|. This helps to validate
  // that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  bool (*MediaGetAudioConfiguration)(
      int output_index,
      CobaltExtensionMediaAudioConfiguration* out_configuration);

  bool (*PlayerGetAudioConfiguration)(
      SbPlayer player, int index,
      CobaltExtensionMediaAudioConfiguration* out_audio_configuration);

} CobaltExtensionConfigurableAudioWriteAheadApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_AUDIO_WRITE_AHEAD_H_
