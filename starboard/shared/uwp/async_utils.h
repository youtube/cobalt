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

#ifndef STARBOARD_SHARED_UWP_ASYNC_UTILS_H_
#define STARBOARD_SHARED_UWP_ASYNC_UTILS_H_

#include <windows.h>

#include <ppltasks.h>

#include "starboard/common/log.h"

using Windows::Foundation::IAsyncAction;
using Windows::Foundation::IAsyncOperation;

namespace starboard {
namespace shared {
namespace uwp {

template <typename TResult>
TResult WaitForResult(IAsyncOperation<TResult>^ operation) {
  using concurrency::task_continuation_context;
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  concurrency::create_task(operation,
                           task_continuation_context::use_arbitrary())
      .then([&event](TResult result) {
        BOOL success = SetEvent(event);
        SB_DCHECK(success);
      }, task_continuation_context::use_arbitrary());
  DWORD return_value = WaitForSingleObject(event, INFINITE);
  SB_DCHECK(return_value == WAIT_OBJECT_0);
  CloseHandle(event);
  return operation->GetResults();
}

inline void WaitForComplete(IAsyncAction^ action) {
  using concurrency::task_continuation_context;
  HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  concurrency::create_task(action, task_continuation_context::use_arbitrary())
      .then(
          [&event](void) {
            BOOL success = SetEvent(event);
            SB_DCHECK(success);
          },
          task_continuation_context::use_arbitrary());
  DWORD return_value = WaitForSingleObject(event, INFINITE);
  SB_DCHECK(return_value == WAIT_OBJECT_0);
  CloseHandle(event);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_ASYNC_UTILS_H_
