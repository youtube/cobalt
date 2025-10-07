// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/platform_service.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_service.h"

using base::android::ScopedJavaLocalRef;

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  const char* name;
  jobject cobalt_service;

  ~CobaltExtensionPlatformServicePrivate() {
    if (name) {
      delete name;
    }
    if (cobalt_service) {
      JNIEnv* env = base::android::AttachCurrentThread();
      env->DeleteGlobalRef(cobalt_service);
    }
  }
} CobaltExtensionPlatformServicePrivate;

namespace starboard {

namespace {

bool Has(const char* name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_name(env,
                                     JniNewStringStandardUTFOrAbort(env, name));
  jboolean j_has = JniCallBooleanMethodOrAbort(
      env, JNIState::GetStarboardBridge(), "hasCobaltService",
      "(Ljava/lang/String;)Z", j_name.obj());
  return j_has;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!Has(name)) {
    SB_LOG(ERROR) << "Can't open Service " << name;
    return kCobaltExtensionPlatformServiceInvalid;
  }
  CobaltExtensionPlatformService service =
      new CobaltExtensionPlatformServicePrivate(
          {context, receive_callback, name});
  ScopedJavaLocalRef<jstring> j_name(env,
                                     JniNewStringStandardUTFOrAbort(env, name));
  jobject cobalt_service = JniCallObjectMethodOrAbort(
      env, JNIState::GetStarboardBridge(), "openCobaltService",
      "(JLjava/lang/String;)Ldev/cobalt/coat/CobaltService;",
      reinterpret_cast<jlong>(service), j_name.obj());
  if (!cobalt_service) {
    delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
    return kCobaltExtensionPlatformServiceInvalid;
  }
  service->cobalt_service = JniConvertLocalRefToGlobalRef(env, cobalt_service);
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  JniCallVoidMethodOrAbort(env, service->cobalt_service, "onClose", "()V");
  ScopedJavaLocalRef<jstring> j_name(
      env, JniNewStringStandardUTFOrAbort(env, service->name));
  JniCallVoidMethodOrAbort(env, JNIState::GetStarboardBridge(),
                           "closeCobaltService", "(Ljava/lang/String;)V",
                           j_name.obj());
  delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
}

void* Send(CobaltExtensionPlatformService service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(data);
  SB_DCHECK(output_length);
  SB_DCHECK(invalid_state);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> data_byte_array(
      env, JniNewByteArrayFromRaw(env, reinterpret_cast<const jbyte*>(data),
                                  length));
  ScopedJavaLocalRef<jobject> j_response_from_client(
      env, static_cast<jbyteArray>(JniCallObjectMethodOrAbort(
               env, service->cobalt_service, "receiveFromClient",
               "([B)Ldev/cobalt/coat/CobaltService$ResponseToClient;",
               data_byte_array.obj())));
  if (!j_response_from_client) {
    *invalid_state = true;
    *output_length = 0;
    return nullptr;
  }
  *invalid_state = JniGetBooleanFieldOrAbort(env, j_response_from_client.obj(),
                                             "invalidState", "Z");
  ScopedJavaLocalRef<jbyteArray> j_out_data_array(
      env, static_cast<jbyteArray>(JniGetObjectFieldOrAbort(
               env, j_response_from_client.obj(), "data", "[B")));
  *output_length = env->GetArrayLength(j_out_data_array.obj());
  char* output = new char[*output_length];
  env->GetByteArrayRegion(j_out_data_array.obj(), 0, *output_length,
                          reinterpret_cast<jbyte*>(output));
  return output;
}

const CobaltExtensionPlatformServiceApi kPlatformServiceApi = {
    kCobaltExtensionPlatformServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltService_nativeSendToClient(JNIEnv* env,
                                                      jobject jcaller,
                                                      jlong nativeService,
                                                      jbyteArray j_data) {
  CobaltExtensionPlatformService service =
      reinterpret_cast<CobaltExtensionPlatformService>(nativeService);
  if (!CobaltExtensionPlatformServiceIsValid(service)) {
    SB_LOG(WARNING) << "Trying to send message through platform service when "
                       "the service is already closed";
    return;
  }

  jsize length = env->GetArrayLength(j_data);
  std::unique_ptr<char[]> data(new char[length]);
  env->GetByteArrayRegion(j_data, 0, length,
                          reinterpret_cast<jbyte*>(data.get()));

  service->receive_callback(service->context, data.get(), length);
}

const void* GetPlatformServiceApiAndroid() {
  return &kPlatformServiceApi;
}

}  // namespace starboard
