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

#include <memory>

#include "cobalt/network/network_system.h"

namespace cobalt {
namespace network {

namespace {

class NetworkSystemStarboard : public NetworkSystem {
 public:
  NetworkSystemStarboard();
};

NetworkSystemStarboard::NetworkSystemStarboard() {}

}  // namespace

std::unique_ptr<NetworkSystem> NetworkSystem::Create(
    base::EventDispatcher* event_dispatcher) {
  return std::unique_ptr<NetworkSystem>(new NetworkSystemStarboard());
}

}  // namespace network
}  // namespace cobalt
