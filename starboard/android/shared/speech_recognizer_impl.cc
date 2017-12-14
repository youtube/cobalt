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

#include "starboard/shared/starboard/speech_recognizer/speech_recognizer_internal.h"

#include <android/native_activity.h>
#include <jni.h>

#include <limits>
#include <vector>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/string.h"

namespace starboard {
namespace android {
namespace shared {

namespace {
// Android speech recognizer error.
// Reference:
// https://developer.android.com/reference/android/speech/SpeechRecognizer.html
enum SpeechRecognizerErrorCode {
  kErrorNetworkTimeout = 1,
  kErrorNetwork = 2,
  kErrorAudio = 3,
  kErrorService = 4,
  kErrorClient = 5,
  kErrorSpeechTimeout = 6,
  kErrorNoMatch = 7,
  kErrorRecognizerBusy = 8,
  kErrorInsufficientPermissions = 9,
};
}  // namespace

class SbSpeechRecognizerImpl : public SbSpeechRecognizerPrivate {
 public:
  explicit SbSpeechRecognizerImpl(const SbSpeechRecognizerHandler* handler);
  ~SbSpeechRecognizerImpl();

  // Called from constructor's thread.
  bool Start(const SbSpeechConfiguration* configuration) override;
  void Stop() override;
  void Cancel() override;

  // Called from Android's UI thread.
  void OnSpeechDetected(bool detected);
  void OnError(int error);
  void OnResults(const std::vector<std::string>& results,
                 const std::vector<float>& confidences,
                 bool is_final);

 private:
  SbSpeechRecognizerHandler handler_;
  jobject j_voice_recognizer_;
  bool is_started_;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;
};

SbSpeechRecognizerImpl::SbSpeechRecognizerImpl(
    const SbSpeechRecognizerHandler* handler)
    : handler_(*handler), is_started_(false) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getVoiceRecognizer", "()Lfoo/cobalt/coat/VoiceRecognizer;");
  j_voice_recognizer_ = env->ConvertLocalRefToGlobalRef(local_ref);
}

SbSpeechRecognizerImpl::~SbSpeechRecognizerImpl() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  Cancel();

  JniEnvExt* env = JniEnvExt::Get();
  env->DeleteGlobalRef(j_voice_recognizer_);
}

bool SbSpeechRecognizerImpl::Start(const SbSpeechConfiguration* configuration) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(configuration);

  if (is_started_) {
    return false;
  }

  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_voice_recognizer_, "startRecognition", "(ZZIJ)V",
      configuration->continuous, configuration->interim_results,
      configuration->max_alternatives, reinterpret_cast<jlong>(this));

  is_started_ = true;
  return true;
}

void SbSpeechRecognizerImpl::Stop() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_started_) {
    return;
  }

  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_voice_recognizer_, "stopRecognition", "()V");

  is_started_ = false;
}

void SbSpeechRecognizerImpl::Cancel() {
  Stop();
}

void SbSpeechRecognizerImpl::OnSpeechDetected(bool detected) {
  // Called from Android's UI thread instead of constructor's thread.
  SB_DCHECK(!thread_checker_.CalledOnValidThread());

  handler_.on_speech_detected(handler_.context, detected);
}

void SbSpeechRecognizerImpl::OnError(int error) {
  // Called from Android's UI thread instead of constructor's thread.
  SB_DCHECK(!thread_checker_.CalledOnValidThread());

  SbSpeechRecognizerError recognizer_error;
  switch (error) {
    case kErrorNetworkTimeout:
      recognizer_error = kSbNetworkError;
      break;
    case kErrorNetwork:
      recognizer_error = kSbNetworkError;
      break;
    case kErrorAudio:
      recognizer_error = kSbAudioCaptureError;
      break;
    case kErrorService:
      recognizer_error = kSbNetworkError;
      break;
    case kErrorClient:
      recognizer_error = kSbAborted;
      break;
    case kErrorSpeechTimeout:
      recognizer_error = kSbNoSpeechError;
      break;
    case kErrorRecognizerBusy:
    case kErrorInsufficientPermissions:
      recognizer_error = kSbNotAllowed;
      break;
    case kErrorNoMatch:
      // Maybe keep listening until found a match or time-out triggered by
      // client.
      recognizer_error = kSbNoSpeechError;
      break;
  }
  handler_.on_error(handler_.context, recognizer_error);
}

void SbSpeechRecognizerImpl::OnResults(const std::vector<std::string>& results,
                                       const std::vector<float>& confidences,
                                       bool is_final) {
  // Called from Android's UI thread instead of constructor's thread.
  SB_DCHECK(!thread_checker_.CalledOnValidThread());

  bool has_confidence = (confidences.size() != 0);
  if (has_confidence) {
    SB_DCHECK(confidences.size() == results.size());
  }
  int kSpeechResultSize = results.size();
  std::vector<SbSpeechResult> speech_results(kSpeechResultSize);
  for (int i = 0; i < kSpeechResultSize; ++i) {
    // The callback is responsible for freeing the buffer with
    // SbMemoryDeallocate.
    speech_results[i].transcript = SbStringDuplicate(results[i].c_str());
    speech_results[i].confidence =
        has_confidence ? confidences[i]
                       : std::numeric_limits<float>::quiet_NaN();
  }
  handler_.on_results(handler_.context, speech_results.data(),
                      kSpeechResultSize, is_final);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace {

using starboard::android::shared::JniEnvExt;

starboard::Mutex s_speech_recognizer_mutex_;
SbSpeechRecognizer s_speech_recognizer = kSbSpeechRecognizerInvalid;
}  // namespace

// static
SbSpeechRecognizer SbSpeechRecognizerPrivate::CreateSpeechRecognizer(
    const SbSpeechRecognizerHandler* handler) {
  starboard::ScopedLock lock(s_speech_recognizer_mutex_);

  SB_DCHECK(!SbSpeechRecognizerIsValid(s_speech_recognizer));
  s_speech_recognizer =
      new starboard::android::shared::SbSpeechRecognizerImpl(handler);
  return s_speech_recognizer;
}

// static
void SbSpeechRecognizerPrivate::DestroySpeechRecognizer(
    SbSpeechRecognizer speech_recognizer) {
  starboard::ScopedLock lock(s_speech_recognizer_mutex_);

  SB_DCHECK(s_speech_recognizer == speech_recognizer);
  SB_DCHECK(SbSpeechRecognizerIsValid(s_speech_recognizer));
  delete s_speech_recognizer;
  s_speech_recognizer = kSbSpeechRecognizerInvalid;
}

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_coat_VoiceRecognizer_nativeOnSpeechDetected(
    JNIEnv* env,
    jobject jcaller,
    jlong nativeSpeechRecognizerImpl,
    jboolean detected) {
  starboard::ScopedLock lock(s_speech_recognizer_mutex_);

  starboard::android::shared::SbSpeechRecognizerImpl* native =
      reinterpret_cast<starboard::android::shared::SbSpeechRecognizerImpl*>(
          nativeSpeechRecognizerImpl);
  // This is called by the Android UI thread and it is possible that the
  // SbSpeechRecognizer is destroyed before this is called.
  if (native != s_speech_recognizer) {
    SB_DLOG(WARNING) << "The speech recognizer is destroyed.";
    return;
  }

  native->OnSpeechDetected(detected);
}

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_coat_VoiceRecognizer_nativeOnError(
    JNIEnv* env,
    jobject jcaller,
    jlong nativeSpeechRecognizerImpl,
    jint error) {
  starboard::ScopedLock lock(s_speech_recognizer_mutex_);

  starboard::android::shared::SbSpeechRecognizerImpl* native =
      reinterpret_cast<starboard::android::shared::SbSpeechRecognizerImpl*>(
          nativeSpeechRecognizerImpl);
  // This is called by the Android UI thread and it is possible that the
  // SbSpeechRecognizer is destroyed before this is called.
  if (native != s_speech_recognizer) {
    SB_DLOG(WARNING) << "The speech recognizer is destroyed.";
    return;
  }

  native->OnError(error);
}

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_coat_VoiceRecognizer_nativeOnResults(
    JniEnvExt* env,
    jobject unused_this,
    jlong nativeSpeechRecognizerImpl,
    jobjectArray results,
    jfloatArray confidences,
    jboolean is_final) {
  starboard::ScopedLock lock(s_speech_recognizer_mutex_);

  starboard::android::shared::SbSpeechRecognizerImpl* native =
      reinterpret_cast<starboard::android::shared::SbSpeechRecognizerImpl*>(
          nativeSpeechRecognizerImpl);
  // This is called by the Android UI thread and it is possible that the
  // SbSpeechRecognizer is destroyed before this is called.
  if (native != s_speech_recognizer) {
    SB_DLOG(WARNING) << "The speech recognizer is destroyed.";
    return;
  }

  std::vector<std::string> options;
  jint argc = env->GetArrayLength(results);
  for (jint i = 0; i < argc; i++) {
    starboard::android::shared::ScopedLocalJavaRef<jstring> element(
        env->GetObjectArrayElement(results, i));
    std::string utf_str = env->GetStringStandardUTFOrAbort(element.Get());
    options.push_back(utf_str);
  }

  std::vector<float> scores(options.size(), 0.0);
  if (confidences != NULL) {
    SB_DCHECK(argc == env->GetArrayLength(results));
    float* confidences_array = env->GetFloatArrayElements(confidences, NULL);
    std::copy(confidences_array, confidences_array + argc, scores.begin());
    env->ReleaseFloatArrayElements(confidences, confidences_array, 0);
  }
  native->OnResults(options, scores, is_final);
}
