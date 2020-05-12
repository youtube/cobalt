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

#include "starboard/system.h"

#include <android/native_activity.h>
#include <jni.h>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"

using starboard::android::shared::ApplicationAndroid;
using starboard::android::shared::JniEnvExt;

namespace {

typedef std::function<void(SbSystemPlatformErrorResponse error_response)>
    SendResponseCallback;

enum {
  // This must be kept in sync with Java dev.cobalt.PlatformError.ErrorType
  kJniErrorTypeConnectionError = 0,
};

}  // namespace

bool SbSystemRaisePlatformError(SbSystemPlatformErrorType type,
                                SbSystemPlatformErrorCallback callback,
                                void* user_data) {
  jint jni_error_type;
  switch (type) {
    case kSbSystemPlatformErrorTypeConnectionError:
      jni_error_type = kJniErrorTypeConnectionError;
      break;
    default:
      SB_NOTREACHED();
      return false;
  }

  JniEnvExt* env = JniEnvExt::Get();

  auto send_response_callback =
      callback ? new SendResponseCallback(
                     [callback,
                      user_data](SbSystemPlatformErrorResponse error_response) {
                       callback(error_response, user_data);
                     })
               : nullptr;

  env->CallStarboardVoidMethodOrAbort(
      "raisePlatformError", "(IJ)V", jni_error_type,
      reinterpret_cast<jlong>(send_response_callback));
  return true;
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_PlatformError_nativeSendResponse(JNIEnv* env,
                                                      jobject unused_this,
                                                      jint jni_response,
                                                      jlong jni_data) {
  SendResponseCallback* send_response_callback =
      reinterpret_cast<SendResponseCallback*>(jni_data);
  if (send_response_callback) {
    SB_DCHECK(*send_response_callback)
        << "send_response_callback should never be an empty function";
    SbSystemPlatformErrorResponse error_response =
        jni_response < 0 ? kSbSystemPlatformErrorResponseNegative :
        jni_response > 0 ? kSbSystemPlatformErrorResponsePositive :
        kSbSystemPlatformErrorResponseCancel;
    (*send_response_callback)(error_response);
    delete send_response_callback;
  }
}
