// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/sbmedia_interface.h"

#include <atomic>

namespace media {

namespace {

std::atomic<SbMediaInterface*> g_sbmedia_interface_for_testing{nullptr};

}  // namespace

SbMediaSupportType DefaultSbMediaInterface::CanPlayMimeAndKeySystem(
    const char* mime,
    const char* key_system) {
  return SbMediaCanPlayMimeAndKeySystem(mime, key_system);
}

SbMediaInterface* GetSbMediaInterface() {
  SbMediaInterface* testing_interface =
      g_sbmedia_interface_for_testing.load(std::memory_order_acquire);
  if (testing_interface) {
    return testing_interface;
  }
  static DefaultSbMediaInterface* default_interface =
      new DefaultSbMediaInterface();
  return default_interface;
}

void SetSbMediaInterfaceForTesting(SbMediaInterface* interface) {
  g_sbmedia_interface_for_testing.store(interface, std::memory_order_release);
}

}  // namespace media
