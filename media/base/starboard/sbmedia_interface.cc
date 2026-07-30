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

#include "media/base/starboard/sbmedia_interface.h"

#include <atomic>

#include "base/check.h"
#include "base/no_destructor.h"

namespace media {

namespace {

std::atomic<SbMediaInterface*> g_sbmedia_interface_for_testing{nullptr};

}  // namespace

SbMediaSupportType DefaultSbMediaInterface::CanPlayMimeAndKeySystem(
    const char* mime,
    const char* key_system) const {
  return SbMediaCanPlayMimeAndKeySystem(mime, key_system);
}

bool DefaultSbMediaInterface::CanChangeType(const char* current_mime,
                                            const char* new_mime) const {
  return SbMediaCanChangeType(current_mime, new_mime);
}

int DefaultSbMediaInterface::GetAudioOutputCount() const {
  return SbMediaGetAudioOutputCount();
}

bool DefaultSbMediaInterface::GetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) const {
  DCHECK(out_configuration);
  if (!out_configuration) {
    return false;
  }
  return SbMediaGetAudioConfiguration(output_index, out_configuration);
}

int DefaultSbMediaInterface::GetBufferAllocationUnit() const {
  return SbMediaGetBufferAllocationUnit();
}

int DefaultSbMediaInterface::GetAudioBufferBudget() const {
  return SbMediaGetAudioBufferBudget();
}

int64_t DefaultSbMediaInterface::GetBufferGarbageCollectionDurationThreshold()
    const {
  return SbMediaGetBufferGarbageCollectionDurationThreshold();
}

int DefaultSbMediaInterface::GetInitialBufferCapacity() const {
  return SbMediaGetInitialBufferCapacity();
}

bool DefaultSbMediaInterface::IsBufferPoolAllocateOnDemand() const {
  return SbMediaIsBufferPoolAllocateOnDemand();
}

int DefaultSbMediaInterface::GetVideoBufferBudget(SbMediaVideoCodec codec,
                                                  int resolution_width,
                                                  int resolution_height,
                                                  int bits_per_pixel) const {
  return SbMediaGetVideoBufferBudget(codec, resolution_width, resolution_height,
                                     bits_per_pixel);
}

SbMediaInterface* GetSbMediaInterface() {
  SbMediaInterface* testing_interface =
      g_sbmedia_interface_for_testing.load(std::memory_order_acquire);
  if (testing_interface) {
    return testing_interface;
  }
  static base::NoDestructor<DefaultSbMediaInterface> default_interface;
  return default_interface.get();
}

void SetSbMediaInterfaceForTesting(SbMediaInterface* interface) {
  g_sbmedia_interface_for_testing.store(interface, std::memory_order_release);
}

}  // namespace media
