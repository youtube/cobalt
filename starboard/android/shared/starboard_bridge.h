// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_STARBOARD_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_STARBOARD_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

// This class serves as a bridge between the native code and Android
// StarboardBridge Java class.
class StarboardBridge {
 public:
  // Returns the singleton.
  static StarboardBridge* GetInstance();

  void Initialize(JNIEnv* env, jobject obj);

  long GetAppStartTimestamp(JNIEnv* env);

  void ApplicationStarted(JNIEnv* env);

  void ApplicationStopping(JNIEnv* env);

  void AfterStopped(JNIEnv* env);

  void AppendArgs(JNIEnv* env, std::vector<std::string>* args_vector);

  std::string GetStartDeepLink(JNIEnv* env);

  ScopedJavaLocalRef<jintArray> GetSupportedHdrTypes(JNIEnv* env);

  void RaisePlatformError(JNIEnv* env, jint errorType, jlong data);

  void RequestSuspend(JNIEnv* env);

  ScopedJavaLocalRef<jobject> GetApplicationContext(JNIEnv* env);

  ScopedJavaGlobalRef<jobject> GetAssetsFromContext(
      JNIEnv* env,
      ScopedJavaLocalRef<jobject>& context);

  std::string GetNativeLibraryDirFromContext(
      JNIEnv* env,
      ScopedJavaLocalRef<jobject>& context);

  std::string GetFilesAbsolutePath(JNIEnv* env);

  std::string GetCacheAbsolutePath(JNIEnv* env);

  ScopedJavaLocalRef<jobject> GetTextToSpeechHelper(JNIEnv* env);

 private:
  StarboardBridge() = default;
  ~StarboardBridge() = default;

  // Prevent copy construction and assignment
  StarboardBridge(const StarboardBridge&) = delete;
  StarboardBridge& operator=(const StarboardBridge&) = delete;

  friend struct base::DefaultSingletonTraits<StarboardBridge>;

  // Java StarboardBridge instance.
  ScopedJavaGlobalRef<jobject> j_starboard_bridge_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_STARBOARD_BRIDGE_H_
