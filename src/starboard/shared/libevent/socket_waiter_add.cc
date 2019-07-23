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

#include "starboard/socket_waiter.h"

#include "starboard/common/log.h"
#include "starboard/shared/libevent/socket_waiter_internal.h"

bool SbSocketWaiterAdd(SbSocketWaiter waiter,
                       SbSocket socket,
                       void* context,
                       SbSocketWaiterCallback callback,
                       int interests,
                       bool persistent) {
  if (!SbSocketWaiterIsValid(waiter)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Waiter (" << waiter << ") is invalid.";
    return false;
  }

  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") is invalid.";
    return false;
  }

  if (!callback) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No callback provided.";
    return false;
  }

  if (!interests) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No interests provided.";
    return false;
  }

  return waiter->Add(socket, context, callback, interests, persistent);
}
