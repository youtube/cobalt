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
#include "starboard/types.h"

int SbBlitterGetMaxContexts(SbBlitterDevice device) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return -1;
  }

  // In the case of DirectFB, there is no limit to the number of contexts that
  // can be made (besides available memory) since contexts in this case are
  // simply state containers.  In other implementations of the Blitter API,
  // it is very reasonable to return 1 here to simplify the implementation,
  // especially threading details.  Most applications, including Cobalt, do not
  // need more than 1 context anyway.
  return INT_MAX;
}
