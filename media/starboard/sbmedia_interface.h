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

#ifndef MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_
#define MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "starboard/media.h"

namespace media {

// SbMediaInterface abstracts the Starboard media C APIs to facilitate testing
// and mocking of media features.
//
// Lifetime and ownership:
// The production instance is a global singleton that is created on demand and
// lives for the lifetime of the process. Custom test implementations can be
// injected via SetSbMediaInterfaceForTesting().
//
// Threading model:
// Implementations must be thread-safe and callable from any thread.
class MEDIA_EXPORT SbMediaInterface {
 public:
  virtual ~SbMediaInterface() = default;

  virtual SbMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* key_system) = 0;
  virtual bool CanChangeType(const char* current_mime,
                             const char* new_mime) = 0;
  virtual int GetAudioOutputCount() = 0;
  virtual bool GetAudioConfiguration(
      int output_index,
      SbMediaAudioConfiguration* out_configuration) = 0;
  virtual int GetBufferAllocationUnit() = 0;
  virtual int GetAudioBufferBudget() = 0;
  virtual int64_t GetBufferGarbageCollectionDurationThreshold() = 0;
  virtual int GetInitialBufferCapacity() = 0;
  virtual bool IsBufferPoolAllocateOnDemand() = 0;
  virtual int GetVideoBufferBudget(SbMediaVideoCodec codec,
                                   int resolution_width,
                                   int resolution_height,
                                   int bits_per_pixel) = 0;
};

// DefaultSbMediaInterface is the production implementation of SbMediaInterface
// that forwards all calls directly to the corresponding Starboard C functions.
//
// Lifetime and ownership:
// Created as a leaked static singleton in GetSbMediaInterface().
//
// Threading model:
// Thread-safe as it delegates to thread-safe Starboard C APIs.
class MEDIA_EXPORT DefaultSbMediaInterface final : public SbMediaInterface {
 public:
  SbMediaSupportType CanPlayMimeAndKeySystem(const char* mime,
                                             const char* key_system) override;
  bool CanChangeType(const char* current_mime, const char* new_mime) override;
  int GetAudioOutputCount() override;
  bool GetAudioConfiguration(
      int output_index,
      SbMediaAudioConfiguration* out_configuration) override;
  int GetBufferAllocationUnit() override;
  int GetAudioBufferBudget() override;
  int64_t GetBufferGarbageCollectionDurationThreshold() override;
  int GetInitialBufferCapacity() override;
  bool IsBufferPoolAllocateOnDemand() override;
  int GetVideoBufferBudget(SbMediaVideoCodec codec,
                           int resolution_width,
                           int resolution_height,
                           int bits_per_pixel) override;
};

// Returns a pointer to the global SbMediaInterface instance.
// By default, this returns a DefaultSbMediaInterface instance.
MEDIA_EXPORT SbMediaInterface* GetSbMediaInterface();

// Sets a custom SbMediaInterface for testing. Pass nullptr to restore the
// default.
MEDIA_EXPORT void SetSbMediaInterfaceForTesting(SbMediaInterface* interface);

}  // namespace media

#endif  // MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_
