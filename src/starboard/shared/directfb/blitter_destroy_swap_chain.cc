// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "starboard/log.h"
#include "starboard/shared/directfb/blitter_internal.h"

SB_EXPORT bool SbBlitterDestroySwapChain(SbBlitterSwapChain swap_chain) {
  if (!SbBlitterIsSwapChainValid(swap_chain)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid swap chain.";
    return false;
  }

  swap_chain->render_target.surface->Release(swap_chain->render_target.surface);

  delete swap_chain;

  return true;
}
