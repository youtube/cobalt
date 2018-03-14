// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "starboard/configuration.h"
#include "starboard/media.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;
using starboard::android::shared::SupportedAudioCodecToMimeType;

SB_EXPORT bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                                       int64_t bitrate) {
  const char* mime = SupportedAudioCodecToMimeType(audio_codec);
  if (!mime) {
    return false;
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
  return env->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaCodecUtil", "hasAudioDecoderFor",
             "(Ljava/lang/String;I)Z", j_mime.Get(),
             static_cast<jint>(bitrate)) == JNI_TRUE;
}
