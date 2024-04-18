// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
#define MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_

#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/location.h"

// Helpers for using base::BindPostTask() with the TaskRunner for the current
// sequence, ie.base::SequencedTaskRunner::GetCurrentDefault().

namespace media {

#if defined(STARBOARD)

template <typename... Args>
inline base::RepeatingCallback<void(Args...)> BindToCurrentLoop(
    base::RepeatingCallback<void(Args...)> cb) {
  return base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(cb), FROM_HERE);
}

template <typename... Args>
inline base::OnceCallback<void(Args...)> BindToCurrentLoop(
    base::OnceCallback<void(Args...)> cb) {
  return base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(cb), FROM_HERE);
}

#else  // defined(STARBOARD)

template <typename... Args>
inline base::RepeatingCallback<void(Args...)> BindToCurrentLoop(
    base::RepeatingCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), location);
}

template <typename... Args>
inline base::OnceCallback<void(Args...)> BindToCurrentLoop(
    base::OnceCallback<void(Args...)> cb,
    const base::Location& location = FROM_HERE) {
  return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                            std::move(cb), location);
}

#endif  // defined(STARBOARD)

}  // namespace media

#endif  // MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
