/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/scoped_operations_batcher.h"

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/rtc_error.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {

ScopedOperationsBatcher::ScopedOperationsBatcher(Thread* target_thread)
    : target_thread_(target_thread) {
  RTC_DCHECK(target_thread_);
}

ScopedOperationsBatcher::~ScopedOperationsBatcher() {
  RTCError error = Run();
  if (!error.ok()) {
    RTC_LOG(LS_ERROR) << "Batcher failed: " << error.message();
  }
}

RTCError ScopedOperationsBatcher::Run() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::vector<FinalizerTask> return_tasks;

  size_t task_idx = 0;
  bool target_thread_is_current = target_thread_->IsCurrent();

  RTCError error = RTCError::OK();
  while (task_idx < tasks_.size()) {
    target_thread_->BlockingCall([&] {
      while (task_idx < tasks_.size()) {
        if (auto* void_task = std::get_if<SimpleBatchTask>(&tasks_[task_idx])) {
          std::move (*void_task)();
        } else {
          auto* returning_task =
              std::get_if<BatchTaskWithFinalizer>(&tasks_[task_idx]);
          RTC_DCHECK(returning_task);
          auto ret = std::move(*returning_task)();
          if (ret.ok()) {
            if (ret.value()) {
              return_tasks.push_back(std::move(ret.value()));
            }
          } else {
            error = ret.MoveError();
          }
        }
        ++task_idx;
        if (!error.ok()) {
          return;
        }
        if (!target_thread_is_current && target_thread_->HasPendingTasks()) {
          return;
        }
      }
    });
    if (!error.ok()) {
      break;
    }
  }

  tasks_.clear();

  for (auto& task : return_tasks) {
    std::move(task)();
  }

  return error;
}

void ScopedOperationsBatcher::Add(SimpleBatchTask task) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task) {
    tasks_.emplace_back(std::move(task));
  }
}

void ScopedOperationsBatcher::AddWithFinalizer(BatchTaskWithFinalizer task) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task) {
    tasks_.emplace_back(std::move(task));
  }
}

}  // namespace webrtc
