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
#include "base/android/jni_array.h"
#include "cobalt/android/jni_headers/CobaltService_jni.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_service.h"

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
  return starboard::StarboardBridge::GetInstance()->HasCobaltService(env, name);
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
  // TODO: b/450024477 - Plumb the Activity through the platform service API.
  // Passing a null Activity is a temporary workaround to avoid breaking the
  // build during the JNI migration, since the Activity is not available in
  // this context.
  auto cobalt_service =
      starboard::StarboardBridge::GetInstance()->OpenCobaltService(
          env, /*activity=*/nullptr, reinterpret_cast<jlong>(service), name);
  if (!cobalt_service) {
    delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
    return kCobaltExtensionPlatformServiceInvalid;
  }
  service->cobalt_service = env->NewGlobalRef(cobalt_service.obj());
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CobaltService_onClose(env, base::android::ScopedJavaLocalRef<jobject>(
                                      env, service->cobalt_service));
  starboard::StarboardBridge::GetInstance()->CloseCobaltService(env,
                                                                service->name);
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
  auto j_data = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(data), length);

  auto j_response = Java_CobaltService_receiveFromClient(
      env,
      base::android::ScopedJavaLocalRef<jobject>(env, service->cobalt_service),
      j_data);

  if (j_response.is_null()) {
    *invalid_state = true;
    *output_length = 0;
    return nullptr;
  }

  auto j_out_data = Java_ResponseToClient_getData(env, j_response);
  int data_length = base::android::SafeGetArrayLength(env, j_out_data);
  SB_CHECK_GE(data_length, 0);
  char* output = new char[data_length];
  env->GetByteArrayRegion(j_out_data.obj(), 0, data_length,
                          reinterpret_cast<jbyte*>(output));

  *invalid_state = Java_ResponseToClient_getInvalidState(env, j_response);
  *output_length = data_length;
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

const void* GetPlatformServiceApiAndroid() {
  return &kPlatformServiceApi;
}

}  // namespace starboard
