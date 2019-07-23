// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <directfb.h>

#include "starboard/blitter.h"
#include "starboard/common/log.h"
#include "starboard/shared/directfb/blitter_internal.h"
#include "starboard/shared/directfb/window_internal.h"

SbBlitterSwapChain SbBlitterCreateSwapChainFromWindow(SbBlitterDevice device,
                                                      SbWindow window) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return kSbBlitterInvalidSwapChain;
  }
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid window.";
    return kSbBlitterInvalidSwapChain;
  }

  starboard::ScopedLock lock(device->mutex);
  SB_DCHECK(device->dfb == window->directfb);

  // Setup our swap chain, which in DirectFB is represented by a surface that
  // has the DSCAPS_PRIMARY | DSCAPS_FLIPPING flags set on it.
  IDirectFBSurface* surface;
  DFBSurfaceDescription primary_dsc;
  primary_dsc.flags = DSDESC_CAPS;
  primary_dsc.caps = static_cast<DFBSurfaceCapabilities>(
      DSCAPS_PRIMARY | DSCAPS_FLIPPING | DSCAPS_STATIC_ALLOC);
  if (device->dfb->CreateSurface(device->dfb, &primary_dsc, &surface) !=
      DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling CreateSurface().";
    return kSbBlitterInvalidSwapChain;
  }

  SbBlitterSwapChainPrivate* swap_chain = new SbBlitterSwapChainPrivate();
  swap_chain->render_target.surface = surface;
  return swap_chain;
}
