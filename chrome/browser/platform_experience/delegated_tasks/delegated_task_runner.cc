// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace platform_experience {

void DelegatedTaskRunner::Run(
    const DelegatedTask& task,
    base::OnceCallback<void(DelegatedTaskResult)> callback) {
  std::move(callback).Run({
      .status = DelegatedTaskStatus::kSuccess,
      .execution_time = base::TimeDelta(),
  });
}

}  // namespace platform_experience
