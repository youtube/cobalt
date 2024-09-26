// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_

#include "base/task/task_traits.h"

namespace update_client {

// Task traits for tasks posted to base::ThreadPool from update_client.

// TODO(crbug.com/1378759) - avoid hardcoding of the task priority.
constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

constexpr base::TaskTraits kTaskTraitsBackgroundDownloader = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_TRAITS_H_
