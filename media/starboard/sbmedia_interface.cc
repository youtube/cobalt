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
#include <string>

namespace media {

namespace {

std::atomic<SbMediaInterface*> g_sbmedia_interface_for_testing{nullptr};

}  // namespace

SbMediaSupportType DefaultSbMediaInterface::CanPlayMimeAndKeySystem(
    std::string_view mime,
    std::string_view key_system) {
  return SbMediaCanPlayMimeAndKeySystem(std::string(mime).c_str(),
                                        std::string(key_system).c_str());
}

bool DefaultSbMediaInterface::CanChangeType(std::string_view current_mime,
                                            std::string_view new_mime) {
  return SbMediaCanChangeType(std::string(current_mime).c_str(),
                              std::string(new_mime).c_str());
}

int DefaultSbMediaInterface::GetAudioOutputCount() {
  return SbMediaGetAudioOutputCount();
}

bool DefaultSbMediaInterface::GetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  return SbMediaGetAudioConfiguration(output_index, out_configuration);
}

int DefaultSbMediaInterface::GetBufferAllocationUnit() {
  return SbMediaGetBufferAllocationUnit();
}

int DefaultSbMediaInterface::GetAudioBufferBudget() {
  return SbMediaGetAudioBufferBudget();
}

int64_t DefaultSbMediaInterface::GetBufferGarbageCollectionDurationThreshold() {
  return SbMediaGetBufferGarbageCollectionDurationThreshold();
}

int DefaultSbMediaInterface::GetInitialBufferCapacity() {
  return SbMediaGetInitialBufferCapacity();
}

bool DefaultSbMediaInterface::IsBufferPoolAllocateOnDemand() {
  return SbMediaIsBufferPoolAllocateOnDemand();
}

int DefaultSbMediaInterface::GetVideoBufferBudget(SbMediaVideoCodec codec,
                                                  int resolution_width,
                                                  int resolution_height,
                                                  int bits_per_pixel) {
  return SbMediaGetVideoBufferBudget(codec, resolution_width, resolution_height,
                                     bits_per_pixel);
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
