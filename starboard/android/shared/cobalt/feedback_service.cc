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

FeedbackService::FeedbackService() = default;

void FeedbackService::SendFeedback(
    bool include_screenshot,
    const script::ValueHandleHolder& product_specific_data,
    script::ExceptionState* exception_state) {
  std::unordered_map<std::string, std::string> product_specific_data_map =
      script::ConvertSimpleObjectToMap(product_specific_data, exception_state);

  // TODO: Incorporate the screenshot.

  // Convert the unordered map of product specific data to a hashmap in JNI.
  starboard::android::shared::JniEnvExt* env =
      starboard::android::shared::JniEnvExt::Get();

  starboard::android::shared::ScopedLocalJavaRef<jobject>
      product_specific_data_hash_map(env->NewObjectOrAbort(
          "java/util/HashMap", "(I)V", product_specific_data_map.size()));

  starboard::android::shared::ScopedLocalJavaRef<jstring> key;
  starboard::android::shared::ScopedLocalJavaRef<jstring> value;

  for (const auto& data : product_specific_data_map) {
    key.Reset(env->NewStringUTF(data.first.c_str()));
    value.Reset(env->NewStringUTF(data.second.c_str()));

    env->CallObjectMethodOrAbort(
        product_specific_data_hash_map.Get(), "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", key.Get(),
        value.Get());
  }

  env->CallStarboardVoidMethodOrAbort("sendFeedback", "(Ljava/util/HashMap;)V",
                                      product_specific_data_hash_map.Get());
}

}  // namespace webapi_extension
}  // namespace cobalt
