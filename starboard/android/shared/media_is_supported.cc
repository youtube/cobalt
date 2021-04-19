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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/media.h"
#include "starboard/string.h"

bool SbMediaIsSupported(SbMediaVideoCodec video_codec,
                        SbMediaAudioCodec audio_codec,
                        const char* key_system) {
  using starboard::android::shared::IsWidevineL1;
  using starboard::android::shared::JniEnvExt;

  if (strchr(key_system, ';')) {
    // TODO: Remove this check and enable key system with attributes support.
    return false;
  }

  // We support all codecs except Opus in L1.  Use allow list to avoid
  // accidentally introducing the support of a codec brought in in future.
  if (audio_codec != kSbMediaAudioCodecNone &&
      audio_codec != kSbMediaAudioCodecAac &&
      audio_codec != kSbMediaAudioCodecAc3 &&
      audio_codec != kSbMediaAudioCodecEac3) {
    return false;
  }
  if (!IsWidevineL1(key_system)) {
    return false;
  }
  return JniEnvExt::Get()->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaDrmBridge",
             "isWidevineCryptoSchemeSupported", "()Z") == JNI_TRUE;
}
