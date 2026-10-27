/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCOPED_OPERATIONS_BATCHER_H_
#define PC_SCOPED_OPERATIONS_BATCHER_H_

#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/sequence_checker.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Batches operations to be executed synchronously on a target thread.
//
// ScopedOperationsBatcher must only be created on the signaling thread.
//
// When the batcher goes out of scope (or `Run()` is explicitly called), it
// executes all queued tasks in one or more `BlockingCall`s to the target
// thread. If the target thread requests a yield during batch execution, the
// batcher will cooperatively yield the thread and resume execution in a
// subsequent `BlockingCall`.
//
// Tasks can either have a `void` return type, or return a new task.
// Any tasks returned by the executed worker thread tasks are collected
// and subsequently executed on the calling thread (typically the signaling
// thread) after the worker thread operations have completed.
class ScopedOperationsBatcher {
 public:
  explicit ScopedOperationsBatcher(Thread* target_thread);
  ~ScopedOperationsBatcher();

  void Run();

  // Queues non-nullptr tasks to be executed on the target thread when the
  // ScopedOperationsBatcher goes out of scope.
  void push_back(absl::AnyInvocable<void() &&> task);
  void push_back(absl::AnyInvocable<absl::AnyInvocable<void() &&>() &&> task);

 private:
  using BatchedTask =
      std::variant<absl::AnyInvocable<void() &&>,
                   absl::AnyInvocable<absl::AnyInvocable<void() &&>() &&>>;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  Thread* const target_thread_;
  std::vector<BatchedTask> tasks_;
};

}  // namespace webrtc

#endif  // PC_SCOPED_OPERATIONS_BATCHER_H_
