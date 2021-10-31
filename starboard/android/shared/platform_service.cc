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

#include "cobalt/extension/platform_service.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  const char* name;
  jobject cobalt_service;

  ~CobaltExtensionPlatformServicePrivate() {
    if (name) {
      delete name;
    }
  }
} CobaltExtensionPlatformServicePrivate;

namespace starboard {
namespace android {
namespace shared {

namespace {

using starboard::android::shared::ScopedLocalJavaRef;
using starboard::android::shared::JniEnvExt;

bool Has(const char* name) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_name(env->NewStringStandardUTFOrAbort(name));
  jboolean j_has = env->CallStarboardBooleanMethodOrAbort(
      "hasCobaltService", "(Ljava/lang/String;)Z", j_name.Get());
  return j_has;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  JniEnvExt* env = JniEnvExt::Get();

  if (!Has(name)) {
    SB_LOG(ERROR) << "Can't open Service " << name;
    return kCobaltExtensionPlatformServiceInvalid;
  }
  CobaltExtensionPlatformService service =
      new CobaltExtensionPlatformServicePrivate(
          {context, receive_callback, name});
  ScopedLocalJavaRef<jstring> j_name(env->NewStringStandardUTFOrAbort(name));
  jobject cobalt_service = env->CallStarboardObjectMethodOrAbort(
      "openCobaltService",
      "(JLjava/lang/String;)Ldev/cobalt/coat/CobaltService;",
      reinterpret_cast<jlong>(service), j_name.Get());
  if (!cobalt_service) {
    delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
    return kCobaltExtensionPlatformServiceInvalid;
  }
  service->cobalt_service = cobalt_service;
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(service->cobalt_service, "onClose", "()V");
  ScopedLocalJavaRef<jstring> j_name(
      env->NewStringStandardUTFOrAbort(service->name));
  env->CallStarboardVoidMethodOrAbort("closeCobaltService",
                                      "(Ljava/lang/String;)V", j_name.Get());
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

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jbyteArray> data_byte_array;
  data_byte_array.Reset(
      env->NewByteArrayFromRaw(reinterpret_cast<const jbyte*>(data), length));
  jobject j_response_from_client =
      static_cast<jbyteArray>(env->CallObjectMethodOrAbort(
          service->cobalt_service, "receiveFromClient",
          "([B)Ldev/cobalt/coat/CobaltService$ResponseToClient;",
          data_byte_array.Get()));
  if (!j_response_from_client) {
    *invalid_state = true;
    *output_length = 0;
    return 0;
  }
  *invalid_state =
      env->GetBooleanFieldOrAbort(j_response_from_client, "invalidState", "Z");
  ScopedLocalJavaRef<jbyteArray> j_out_data_array(static_cast<jbyteArray>(
      env->GetObjectFieldOrAbort(j_response_from_client, "data", "[B")));
  *output_length = env->GetArrayLength(j_out_data_array.Get());
  char* output = new char[*output_length];
  env->GetByteArrayRegion(j_out_data_array.Get(), 0, *output_length,
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
Java_dev_cobalt_coat_CobaltService_nativeSendToClient(JniEnvExt* env,
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
  char* data = new char[length];
  env->GetByteArrayRegion(j_data, 0, length, reinterpret_cast<jbyte*>(data));

  service->receive_callback(service->context, data, length);
}

const void* GetPlatformServiceApi() {
  return &kPlatformServiceApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
