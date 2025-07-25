// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_FOREGROUND_TASK_RUNNER_H_
#define GIN_V8_FOREGROUND_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"
#include "gin/v8_foreground_task_runner_base.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gin {

class V8ForegroundTaskRunner : public V8ForegroundTaskRunnerBase {
 public:
  V8ForegroundTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~V8ForegroundTaskRunner() override;

  // v8::Platform implementation.
  void PostTask(std::unique_ptr<v8::Task> task) override;

  void PostNonNestableTask(std::unique_ptr<v8::Task> task) override;

  void PostDelayedTask(std::unique_ptr<v8::Task> task,
                       double delay_in_seconds) override;

  void PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                  double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override;

  bool NonNestableTasksEnabled() const override;
  bool NonNestableDelayedTasksEnabled() const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gin

#endif  // GIN_V8_FOREGROUND_TASK_RUNNER_H_
