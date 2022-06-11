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

#include "cobalt/worker/window_client.h"

namespace cobalt {
namespace worker {

WindowClient::WindowClient(const WindowData& window_data)
    : Client(window_data.client) {
  // Algorithm for Create Window Client:
  //   https://w3c.github.io/ServiceWorker/#create-window-client

  // 1. Let windowClient be a new WindowClient object.
  // 2. Set windowClient’s service worker client to client.
  // Passed to Client constructor.

  // 3. Set windowClient’s frame type to frameType.
  set_frame_type(window_data.frame_type);

  // 4. Set windowClient’s visibility state to visibilityState.
  // 5. Set windowClient’s focus state to focusState.
  // TODO(b/235838698): Implement WindowCLient properties and methods.

  // 6. Set windowClient’s ancestor origins array to a frozen array created from
  //    ancestorOriginsList.
  // Not supported by Cobalt.

  // 7. Return windowClient.
}

}  // namespace worker
}  // namespace cobalt
