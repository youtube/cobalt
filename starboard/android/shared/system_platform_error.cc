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

using starboard::android::shared::ApplicationAndroid;

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
  kJniErrorTypeUnknown = -1,
  kJniErrorTypeConnectionError = 0,
  kJniErrorTypeUserSignedOut = 1,
  kJniErrorTypeUserAgeRestricted = 2
};

jobject JniRaisePlatformError(ANativeActivity* activity,
                              jint jni_error_type, jlong jni_data) {
  JNIEnv* env;
  activity->vm->AttachCurrentThread(&env, 0);

  jobject activity_obj = activity->clazz;
  jclass activity_class = env->GetObjectClass(activity_obj);
  jmethodID method = env->GetMethodID(activity_class,
      "raisePlatformError", "(IJ)Lfoo/cobalt/PlatformError;");
  jobject error_obj = env->NewGlobalRef(
      env->CallObjectMethod(activity_obj, method, jni_error_type, jni_data));

  activity->vm->DetachCurrentThread();

  return error_obj;
}

void JniClearPlatformError(ANativeActivity* activity, jobject error_obj) {
  JNIEnv* env;
  activity->vm->AttachCurrentThread(&env, 0);

  jclass error_class = env->GetObjectClass(error_obj);
  jmethodID method = env->GetMethodID(error_class, "clear", "()V");
  env->CallObjectMethod(error_obj, method);

  activity->vm->DetachCurrentThread();
}

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
    case kSbSystemPlatformErrorTypeUserSignedOut:
      jni_error_type = kJniErrorTypeUserSignedOut;
      break;
    case kSbSystemPlatformErrorTypeUserAgeRestricted:
      jni_error_type = kJniErrorTypeUserAgeRestricted;
      break;
    default:
      jni_error_type = kJniErrorTypeUnknown;
  }

  ANativeActivity* activity = ApplicationAndroid::Get()->GetActivity();
  SbSystemPlatformError error_handle =
      new SbSystemPlatformErrorPrivate(type, callback, user_data);
  error_handle->error_obj = JniRaisePlatformError(
      activity, jni_error_type, reinterpret_cast<jlong>(error_handle));
  return error_handle;
}

void SbSystemClearPlatformError(SbSystemPlatformError handle) {
  JniClearPlatformError(ApplicationAndroid::Get()->GetActivity(),
                        handle->error_obj);
}

extern "C" SB_EXPORT_PLATFORM void Java_foo_cobalt_PlatformError_onCleared(
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
