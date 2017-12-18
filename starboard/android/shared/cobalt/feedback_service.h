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

#ifndef STARBOARD_ANDROID_SHARED_COBALT_FEEDBACK_SERVICE_H_
#define STARBOARD_ANDROID_SHARED_COBALT_FEEDBACK_SERVICE_H_

#include <string>

#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace webapi_extension {

class FeedbackService : public ::cobalt::script::Wrappable {
 public:
  FeedbackService();

  const void SendFeedbackWithSequence(
      const bool includeScreenshot,
      const ::cobalt::script::Sequence<std::string>& productSpecificData) const;

  DEFINE_WRAPPABLE_TYPE(FeedbackService);

 private:
  ~FeedbackService() override;

  DISALLOW_COPY_AND_ASSIGN(FeedbackService);
};

}  // namespace webapi_extension
}  // namespace cobalt

#endif  // STARBOARD_ANDROID_SHARED_COBALT_FEEDBACK_SERVICE_H_
