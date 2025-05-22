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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_int_wrapper.h"

namespace starboard {
namespace android {
namespace shared {

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

class MediaDrmBridge {
 public:
  static jboolean IsSuccess(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj);
  static ScopedJavaLocalRef<jstring> GetErrorMessage(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
  static ScopedJavaLocalRef<jobject> CreateJavaMediaDrmBridge(
      JNIEnv* env,
      const JavaRef<jstring>& keySystem,
      jlong nativeMediaDrmBridge);
  static bool IsWidevineSupported(JNIEnv* env);
  static bool IsCbcsSupported(JNIEnv* env);
  static void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& obj);
  static void CreateSession(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            JniIntWrapper ticket,
                            const base::android::JavaRef<jbyteArray>& initData,
                            const base::android::JavaRef<jstring>& mime);
  static ScopedJavaLocalRef<jobject> UpdateSession(
      JNIEnv* env,
      const JavaRef<jobject>& obj,
      JniIntWrapper ticket,
      const JavaRef<jbyteArray>& sessionId,
      const JavaRef<jbyteArray>& response);
  static void CloseSession(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj,
                           const base::android::JavaRef<jbyteArray>& sessionId);
  static base::android::ScopedJavaLocalRef<jbyteArray> GetMetricsInBase64(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
  static base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
  static bool CreateMediaCryptoSession(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
