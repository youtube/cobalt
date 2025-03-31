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

<<<<<<< HEAD
#include "starboard/android/shared/accessibility_extension.h"

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/memory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

=======
>>>>>>> bf790234113 (Mojom implementation of window.h5vcc.accessibility (#5214))
namespace starboard {
namespace android {
namespace shared {
namespace accessibility {

<<<<<<< HEAD
// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

=======
namespace accessibility {
// ATV no longer supports GetTextToSpeechSettings in Chrobalt,
// Please reference starboard/android/shared/text_to_speech_helper.h for
// supported text-to-speech APIs instead.
>>>>>>> bf790234113 (Mojom implementation of window.h5vcc.accessibility (#5214))
bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  return false;
}

}  // namespace accessibility
}  // namespace shared
}  // namespace android
}  // namespace starboard
