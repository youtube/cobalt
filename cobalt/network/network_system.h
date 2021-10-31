// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_NETWORK_NETWORK_SYSTEM_H_
#define COBALT_NETWORK_NETWORK_SYSTEM_H_

#include <memory>

#include "base/basictypes.h"
#include "cobalt/base/event_dispatcher.h"

namespace cobalt {
namespace network {

// Platform-specific networking initialization and management.
// The NetworkSystem will be owned by NetworkModule.
class NetworkSystem {
 public:
  virtual ~NetworkSystem() {}

  // To be implemented by each platform.
  // Platforms may wish to dispatch NetworkEvents to any registered listeners.
  // Use the event_dispatcher for this.
  static std::unique_ptr<NetworkSystem> Create(
      base::EventDispatcher* event_dispatcher);

 protected:
  NetworkSystem() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkSystem);
};

}  // namespace network
}  // namespace cobalt
#endif  // COBALT_NETWORK_NETWORK_SYSTEM_H_
