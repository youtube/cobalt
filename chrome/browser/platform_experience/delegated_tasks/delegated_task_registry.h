// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_REGISTRY_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_REGISTRY_H_

#include "base/memory/raw_span.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

namespace platform_experience {

// Structure holding static metadata for a registered delegated task.
struct DelegatedTaskMetadata {
  // Task name is used for metric logging purposes and should not be renamed
  // once added.
  const char* task_name;
  // List of task specific command line switch keys that are used to pass
  // arbitrary data with the task.
  base::raw_span<const char* const> cmdline_switch_keys;
};

DelegatedTaskMetadata GetDelegatedTaskMetadata(DelegatedTaskType type);

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_REGISTRY_H_
