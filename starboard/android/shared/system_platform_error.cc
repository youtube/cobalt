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

#include "starboard/system.h"

#include <android/native_activity.h>
#include <jni.h>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"

using starboard::android::shared::ApplicationAndroid;
using starboard::android::shared::JniEnvExt;

struct SbSystemPlatformErrorPrivate {
  SbSystemPlatformErrorPrivate(SbSystemPlatformErrorType type,
                               SbSystemPlatformErrorCallback callback,
                               void* user_data)
      : error_obj(NULL), type(type), callback(callback), user_data(user_data) {}

  jobject error_obj;  // global ref to foo.cobalt.PlatformError instance
  SbSystemPlatformErrorType type;
  SbSystemPlatformErrorCallback callback;
  void* user_data;
};

namespace {

enum {
  // This must be kept in sync with Java foo.cobalt.PlatformError.ErrorType
  kJniErrorTypeConnectionError = 0,
};

}  // namespace

SbSystemPlatformError SbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data) {
  jint jni_error_type;
  switch (type) {
    case kSbSystemPlatformErrorTypeConnectionError:
      jni_error_type = kJniErrorTypeConnectionError;
      break;
    default:
      SB_NOTREACHED();
      return kSbSystemPlatformErrorInvalid;
  }

  SbSystemPlatformError error_handle =
      new SbSystemPlatformErrorPrivate(type, callback, user_data);

  JniEnvExt* env = JniEnvExt::Get();
  jobject error_obj = env->CallActivityObjectMethodOrAbort(
      "raisePlatformError", "(IJ)Lfoo/cobalt/coat/PlatformError;",
      jni_error_type, reinterpret_cast<jlong>(error_handle));
  error_handle->error_obj = env->NewGlobalRef(error_obj);

  return error_handle;
}

void SbSystemClearPlatformError(SbSystemPlatformError handle) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallObjectMethodOrAbort(handle->error_obj, "clear", "()V");
}

extern "C" SB_EXPORT_PLATFORM
void Java_foo_cobalt_coat_PlatformError_onCleared(
    JNIEnv* env, jobject unused_this, jint jni_response, jlong jni_data) {
  SB_UNREFERENCED_PARAMETER(unused_this);
  SbSystemPlatformError error_handle =
      reinterpret_cast<SbSystemPlatformError>(jni_data);
  env->DeleteGlobalRef(error_handle->error_obj);
  if (error_handle->callback) {
    SbSystemPlatformErrorResponse error_response =
        jni_response < 0 ? kSbSystemPlatformErrorResponseNegative :
        jni_response > 0 ? kSbSystemPlatformErrorResponsePositive :
        kSbSystemPlatformErrorResponseCancel;
    error_handle->callback(error_response, error_handle->user_data);
  }
}
