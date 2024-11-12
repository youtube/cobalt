// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

#include "base/task/single_thread_task_runner.h"

namespace blink {

IOTaskRunnerTestingPlatformSupport::IOTaskRunnerTestingPlatformSupport()
    : io_thread_(NonMainThread::CreateThread(
          ThreadCreationParams(ThreadType::kTestThread))) {}

scoped_refptr<base::SingleThreadTaskRunner>
IOTaskRunnerTestingPlatformSupport::GetIOTaskRunner() const {
  return io_thread_->GetTaskRunner();
}

}  // namespace blink
