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
#include "starboard/log.h"
#include "starboard/once.h"
#include "starboard/string.h"

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

bool is_initialized = false;

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
  env->CallVoidMethodOrAbort(service->cobalt_service, "close", "()V");
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
  jbyteArray j_out_data_array = static_cast<jbyteArray>(
      env->GetObjectFieldOrAbort(j_response_from_client, "data", "[B"));
  *output_length = env->GetArrayLength(j_out_data_array);
  char* output = new char[*output_length];
  env->GetByteArrayRegion(j_out_data_array, 0, *output_length,
                          reinterpret_cast<jbyte*>(output));
  return output;
}

SB_ONCE_INITIALIZE_FUNCTION(CobaltExtensionPlatformServiceApi,
                            GetOrCreatePlatformServiceApi);
}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltService_nativeSendToClient(JniEnvExt* env,
                                                      jobject jcaller,
                                                      jlong nativeService,
                                                      jbyteArray j_data) {
  CobaltExtensionPlatformService service =
      reinterpret_cast<CobaltExtensionPlatformService>(nativeService);
  SB_DCHECK(CobaltExtensionPlatformServiceIsValid(service));

  jsize length = env->GetArrayLength(j_data);
  char* data = new char[length];
  env->GetByteArrayRegion(j_data, 0, length, reinterpret_cast<jbyte*>(data));

  service->receive_callback(service->context, data, length);
}

void* GetPlatformServiceApi() {
  CobaltExtensionPlatformServiceApi* platform_service_api =
      GetOrCreatePlatformServiceApi();
  if (!is_initialized) {
    platform_service_api->kName = kCobaltExtensionPlatformServiceName;
    platform_service_api->kVersion = kPlatformServiceExtensionVersion;
    platform_service_api->Has = &Has;
    platform_service_api->Open = &Open;
    platform_service_api->Close = &Close;
    platform_service_api->Send = &Send;

    is_initialized = true;
  }
  return platform_service_api;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
