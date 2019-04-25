// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Similar to directfb/blitter_internal.h.

#ifndef STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_
#define STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_

#include <EGL/egl.h>

#include "starboard/blitter.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"

struct SbBlitterDevicePrivate {
  // Internally we store our EGLDisplay object inside of the SbBlitterDevice
  // object.
  EGLDisplay display;

  // Internally we store our EGLConfig object inside of the SbBlitterDevice
  // object.
  EGLConfig config;

  // Mutex to ensure thread-safety in all SbBlitterDevice-related function
  // calls.
  starboard::Mutex mutex;
};

namespace starboard {
namespace shared {
namespace blittergles {

struct SbBlitterDeviceRegistry {
  // This implementation only supports a single default device, so we remember
  // it here.
  SbBlitterDevicePrivate* default_device;

  // The mutex is necessary since SbBlitterDeviceRegistry is a global structure
  // that must be accessed by any thread to create/destroy devices.
  starboard::Mutex mutex;
};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry();

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_
