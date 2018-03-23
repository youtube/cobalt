// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/cobalt/feedback_service.h"

#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"

namespace cobalt {
namespace webapi_extension {

void FeedbackService::SendFeedback(
    const script::ValueHandleHolder& product_specific_data,
    script::ExceptionState* exception_state) {
  static const scoped_refptr<dom::ArrayBuffer> kEmptyArrayBuffer;
  SendFeedback(product_specific_data, kEmptyArrayBuffer, exception_state);
}

void FeedbackService::SendFeedback(
    const script::ValueHandleHolder& product_specific_data,
    const scoped_refptr<dom::ArrayBuffer>& screenshot_data,
    script::ExceptionState* exception_state) {
  using starboard::android::shared::ScopedLocalJavaRef;
  using starboard::android::shared::JniEnvExt;

  std::unordered_map<std::string, std::string> product_specific_data_map =
      script::ConvertSimpleObjectToMap(product_specific_data, exception_state);

  // Convert the unordered map of product specific data to a hashmap in JNI.
  JniEnvExt* env = JniEnvExt::Get();

  ScopedLocalJavaRef<jobject> product_specific_data_hash_map(
      env->NewObjectOrAbort("java/util/HashMap", "(I)V",
                            product_specific_data_map.size()));

  ScopedLocalJavaRef<jstring> key;
  ScopedLocalJavaRef<jstring> value;

  for (const auto& data : product_specific_data_map) {
    key.Reset(env->NewStringUTF(data.first.c_str()));
    value.Reset(env->NewStringUTF(data.second.c_str()));

    env->CallObjectMethodOrAbort(
        product_specific_data_hash_map.Get(), "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", key.Get(),
        value.Get());
  }

  ScopedLocalJavaRef<jbyteArray> byte_array;
  if (screenshot_data) {
    byte_array.Reset(env->NewByteArrayFromRaw(
        reinterpret_cast<const jbyte*>(screenshot_data->data()),
        screenshot_data->byte_length()));
    env->AbortOnException();
  }

  env->CallStarboardVoidMethodOrAbort(
      "sendFeedback", "(Ljava/util/HashMap;[B)V",
      product_specific_data_hash_map.Get(), byte_array.Get());
}

}  // namespace webapi_extension
}  // namespace cobalt
