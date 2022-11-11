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

#ifndef COBALT_WORKER_WINDOW_CLIENT_H_
#define COBALT_WORKER_WINDOW_CLIENT_H_

#include "cobalt/dom/visibility_state.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/client.h"
#include "cobalt/worker/frame_type.h"

namespace cobalt {
namespace worker {

struct WindowData {
 public:
  WindowData(web::EnvironmentSettings* client = nullptr,
             FrameType frame_type = kFrameTypeTopLevel)
      : client(client), frame_type(frame_type) {}

  web::EnvironmentSettings* client = nullptr;
  FrameType frame_type = kFrameTypeTopLevel;
};

class WindowClient : public Client {
 public:
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-window-client
  static scoped_refptr<Client> Create(const WindowData& window_data) {
    return new WindowClient(window_data);
  }
  dom::VisibilityState visibility_state();
  bool focused();

  // TODO(b/235838698): Implement WindowClient methods.
  DEFINE_WRAPPABLE_TYPE(WindowClient);

 private:
  explicit WindowClient(const WindowData& window_data);

  dom::VisibilityState visibility_state_ = dom::kVisibilityStateVisible;
  bool focused_ = true;
};


}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WINDOW_CLIENT_H_
