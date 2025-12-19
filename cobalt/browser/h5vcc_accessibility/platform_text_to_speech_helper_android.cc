// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper_android.h"

#include "base/android/jni_android.h"
#include "starboard/android/shared/text_to_speech_helper.h"

namespace h5vcc_accessibility {

PlatformTextToSpeechHelperAndroid::PlatformTextToSpeechHelperAndroid(
    base::WeakPtr<PlatformTextToSpeechHelper::Client> client)
    : PlatformTextToSpeechHelper(client) {
  cobalt_text_to_speech_observer_.Observe(
      starboard::CobaltTextToSpeechHelper::GetInstance());
}

PlatformTextToSpeechHelperAndroid::~PlatformTextToSpeechHelperAndroid() =
    default;

bool PlatformTextToSpeechHelperAndroid::IsTextToSpeechEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* cobalt_tts_helper = starboard::CobaltTextToSpeechHelper::GetInstance();
  cobalt_tts_helper->Initialize(env);
  return cobalt_tts_helper->IsTextToSpeechEnabled(env);
}

void PlatformTextToSpeechHelperAndroid::ObserveTextToSpeechChange() {
  NotifyTextToSpeechChange();
}

}  // namespace h5vcc_accessibility
