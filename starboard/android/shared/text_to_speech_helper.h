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

#ifndef STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_HELPER_H_
#define STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "starboard/android/shared/text_to_speech_observer.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;

class CobaltTextToSpeechHelper {
 public:
  // Return the singleton.
  static CobaltTextToSpeechHelper* GetInstance();
  void Initialize(JNIEnv* env);

  void AddObserver(TextToSpeechObserver* observer);
  void RemoveObserver(TextToSpeechObserver* observer);

  bool IsTextToSpeechEnabled(JNIEnv* env) const;
  void SendTextToSpeechChangeEvent() const;

 private:
  friend struct base::DefaultSingletonTraits<CobaltTextToSpeechHelper>;
  // Java CobaltTextToSpeechHelper instance.
  ScopedJavaGlobalRef<jobject> j_text_to_speech_helper_;

  CobaltTextToSpeechHelper() = default;
  ~CobaltTextToSpeechHelper() = default;

  // Thread-safe observer list for H5vccAccessibilityImpl.
  base::ObserverList<TextToSpeechObserver> observers_
      GUARDED_BY(observers_lock_);
  mutable base::Lock observers_lock_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_HELPER_H_
