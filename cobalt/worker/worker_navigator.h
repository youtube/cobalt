// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_WORKER_NAVIGATOR_H_
#define COBALT_WORKER_WORKER_NAVIGATOR_H_

#include <string>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/navigator_ua_data.h"

namespace cobalt {
namespace worker {

// The WorkerNavigator object represents the identity and state of the user
// agent (the client).
// https://html.spec.whatwg.org/commit-snapshots/9fe65ccaf193c8c0d93fd3638b4c8e935fc28213/#the-workernavigator-object

class WorkerNavigator : public web::NavigatorBase {
 public:
  explicit WorkerNavigator(script::EnvironmentSettings* settings);
  WorkerNavigator(const WorkerNavigator&) = delete;
  WorkerNavigator& operator=(const WorkerNavigator&) = delete;

  DEFINE_WRAPPABLE_TYPE(WorkerNavigator);

 private:
  ~WorkerNavigator() override {}
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_NAVIGATOR_H_
