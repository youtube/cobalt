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

#ifndef STARBOARD_ANDROID_SHARED_COBALT_ANDROID_H_
#define STARBOARD_ANDROID_SHARED_COBALT_ANDROID_H_

#include "cobalt/dom/window.h"
#include "cobalt/script/wrappable.h"
#include "starboard/android/shared/cobalt/feedback_service.h"

namespace cobalt {
namespace webapi_extension {

class Android : public ::cobalt::script::Wrappable {
 public:
  explicit Android(const scoped_refptr<::cobalt::dom::Window>& window);

  const scoped_refptr<FeedbackService>& feedback_service() const {
    return feedback_service_;
  }

  DEFINE_WRAPPABLE_TYPE(Android);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~Android() override;

  scoped_refptr<FeedbackService> feedback_service_;

  DISALLOW_COPY_AND_ASSIGN(Android);
};

}  // namespace webapi_extension
}  // namespace cobalt

#endif  // STARBOARD_ANDROID_SHARED_COBALT_ANDROID_H_
