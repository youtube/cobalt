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
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_service.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

/**
 * Holds the response data and state returned to the client.
 * This struct is used to transfer response information from Java to C++ via
 * JNI. It is owned by the caller and can be used from any thread.
 */
struct ResponseToClientInfo {
  std::vector<uint8_t> data;
  bool invalid_state = false;
};

}  // namespace starboard

namespace jni_zero {
template <>
starboard::ResponseToClientInfo FromJniType<starboard::ResponseToClientInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& j_response);
}  // namespace jni_zero

// CobaltService_jni.h must be included AFTER the ResponseToClientInfo struct
// and its FromJniType specialization declaration because the generated JNI
// helper functions return ResponseToClientInfo by value and invoke FromJniType
// implicitly.
#include "cobalt/android/jni_headers/CobaltService_jni.h"

namespace jni_zero {
template <>
starboard::ResponseToClientInfo FromJniType<starboard::ResponseToClientInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& j_response) {
  starboard::ResponseToClientInfo info;
  if (base::android::HasException(env)) {
    jthrowable raw_throwable =
        static_cast<jthrowable>(env->ExceptionOccurred());
    auto throwable =
        jni_zero::ScopedJavaLocalRef<jthrowable>::Adopt(env, raw_throwable);
    base::android::ClearException(env);
    std::string exception_info =
        base::android::GetJavaExceptionInfo(env, throwable);
    info.data.assign(exception_info.begin(), exception_info.end());
    info.invalid_state = true;
    return info;
  }
  if (!j_response) {
    info.invalid_state = true;
    return info;
  }
  info.invalid_state =
      starboard::Java_ResponseToClient_getInvalidState(env, j_response);
  if (!info.invalid_state) {
    ScopedJavaLocalRef<jbyteArray> j_data =
        starboard::Java_ResponseToClient_getData(env, j_response);
    if (j_data) {
      base::android::JavaByteArrayToByteVector(env, j_data, &info.data);
    }
  }
  return info;
}
}  // namespace jni_zero

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  std::string name;
  jni_zero::ScopedJavaGlobalRef<jobject> cobalt_service;

  ~CobaltExtensionPlatformServicePrivate() = default;
} CobaltExtensionPlatformServicePrivate;

namespace starboard {

namespace {

using jni_zero::AttachCurrentThread;

bool Has(const char* name) {
  JNIEnv* env = AttachCurrentThread();
  return starboard::StarboardBridge::GetInstance()->HasCobaltService(env, name);
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  JNIEnv* env = AttachCurrentThread();

  if (!Has(name)) {
    SB_LOG(ERROR) << "Can't open Service " << name;
    return kCobaltExtensionPlatformServiceInvalid;
  }

  CobaltExtensionPlatformService service =
      new CobaltExtensionPlatformServicePrivate(
          {context, receive_callback, name});
  auto cobalt_service =
      starboard::StarboardBridge::GetInstance()->OpenCobaltService(
          env, reinterpret_cast<jlong>(service), name);
  if (!cobalt_service) {
    delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
    return kCobaltExtensionPlatformServiceInvalid;
  }
  service->cobalt_service.Reset(cobalt_service);
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  if (!service || !service->cobalt_service) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_CobaltService_onClose(env, service->cobalt_service);

  starboard::StarboardBridge::GetInstance()->CloseCobaltService(
      env, service->name.c_str());
  delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
}

void* Send(CobaltExtensionPlatformService service,
           const void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(length == 0 || data);
  SB_DCHECK(output_length);
  SB_DCHECK(invalid_state);

  if (!service || service->cobalt_service.is_null()) {
    SB_LOG(ERROR) << "Send failed: Service or Java handle is null.";
    *invalid_state = true;
    return nullptr;
  }

  JNIEnv* env = AttachCurrentThread();
  auto j_data = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(data), length);
  ResponseToClientInfo response_info = Java_CobaltService_receiveFromClient(
      env, service->cobalt_service, j_data);
  *invalid_state = response_info.invalid_state;
  *output_length = response_info.data.size();

  if (response_info.data.empty()) {
    *output_length = 0;
    return nullptr;
  }

  void* output = malloc(response_info.data.size());
  if (!output) {
    *output_length = 0;
    return nullptr;
  }
  memcpy(output, response_info.data.data(), response_info.data.size());
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

void JNI_CobaltService_NativeSendToClient(
    JNIEnv* env,
    jlong nativeService,
    const jni_zero::JavaParamRef<jbyteArray>& j_data) {
  auto* service =
      reinterpret_cast<CobaltExtensionPlatformServicePrivate*>(nativeService);

  if (!service) {
    SB_LOG(ERROR) << "NativeSendToClient called with null service pointer.";
    return;
  }

  if (!service->receive_callback) {
    SB_LOG(ERROR) << "Service " << service->name << " has no receive callback.";
    return;
  }

  // Convert Java byte array to a C++ vector.
  std::vector<uint8_t> data;
  base::android::JavaByteArrayToByteVector(env, j_data, &data);

  // Pass the data back to the Cobalt/Starboard layer.
  service->receive_callback(service->context, data.data(), data.size());
}

}  // namespace starboard
