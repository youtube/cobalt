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

namespace cobalt {
namespace webapi_extension {

FeedbackService::FeedbackService() = default;

void FeedbackService::SendFeedback(
    bool include_screenshot,
    const script::ValueHandleHolder& product_specific_data,
    script::ExceptionState* exception_state) {
  std::unordered_map<std::string, std::string> product_specific_data_map =
      script::ConvertSimpleObjectToMap(product_specific_data, exception_state);

  // TODO: Pass to Java.
}

}  // namespace webapi_extension
}  // namespace cobalt
