// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_

#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"

// Helpers for using base::BindPostTask() with the TaskRunner for the current
// sequence, ie. base::SequencedTaskRunnerHandle::Get().

namespace media_m96 {

#if defined(STARBOARD)

template <typename... Args>
inline base::RepeatingCallback<void(Args...)> base::BindPostTaskToCurrentDefault(
    base::RepeatingCallback<void(Args...)> cb) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), FROM_HERE);
}

template <typename... Args>
inline base::OnceCallback<void(Args...)> base::BindPostTaskToCurrentDefault(
    base::OnceCallback<void(Args...)> cb) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), FROM_HERE);
}

#else  // defined(STARBOARD)

template <typename... Args>
inline base::RepeatingCallback<void(Args...)> base::BindPostTaskToCurrentDefault(
    base::RepeatingCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), location);
}

template <typename... Args>
inline base::OnceCallback<void(Args...)> base::BindPostTaskToCurrentDefault(
    base::OnceCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), location);
}

#endif  // defined(STARBOARD)

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
