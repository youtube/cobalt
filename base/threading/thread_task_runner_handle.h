#ifndef BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_
#define BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"

namespace base {

class ThreadTaskRunnerHandle {
 public:
  static const scoped_refptr<SingleThreadTaskRunner>& Get();
 private:
  scoped_refptr<SingleThreadTaskRunner> task_runner_;
};

}

#endif
