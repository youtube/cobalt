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

#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/client.h"
#include "cobalt/worker/frame_type.h"

namespace cobalt {
namespace worker {

struct WindowData {
  web::EnvironmentSettings* client = nullptr;
  FrameType frame_type = kFrameTypeTopLevel;
};

class WindowClient : public Client {
 public:
  // https://w3c.github.io/ServiceWorker/#create-window-client
  static WindowClient* Create(const WindowData& window_data) {
    return new WindowClient(window_data);
  }
  // TODO(b/235838698): Implement WindowCLient properties and methods.

  DEFINE_WRAPPABLE_TYPE(WindowClient);

 private:
  explicit WindowClient(const WindowData& window_data);
};


}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WINDOW_CLIENT_H_
