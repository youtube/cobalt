// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

#include "base/time/time.h"

namespace platform_experience {

base::TimeDelta DelegatedTask::GetTimeout() const {
  return base::Seconds(10);
}

}  // namespace platform_experience
