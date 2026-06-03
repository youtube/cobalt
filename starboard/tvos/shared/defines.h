// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_DEFINES_H_
#define STARBOARD_TVOS_SHARED_DEFINES_H_

#import <Foundation/Foundation.h>

/**
 *  @brief Helper function to ensure a block is run on the host application's UI
 *      thread. Blocks the current thread until @c block has completed.
 */
inline void onApplicationMainThread(void (^block)(void)) {
  if (![NSThread isMainThread]) {
    dispatch_sync(dispatch_get_main_queue(), block);
  } else {
    block();
  }
}

/**
 *  @brief Helper function to ensure a block is run on the host application's UI
 *      thread. Execute @c block immediately if currently on the main thread,
 *      otherwise dispatch an async task.
 */
inline void onApplicationMainThreadAsync(void (^block)(void)) {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), block);
  } else {
    block();
  }
}

#endif  // STARBOARD_TVOS_SHARED_DEFINES_H_
