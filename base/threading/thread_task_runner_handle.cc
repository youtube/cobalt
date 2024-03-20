#include "base/threading/thread_task_runner_handle.h"

namespace base {
namespace {

base::LazyInstance<base::ThreadLocalOwnedPointer<ThreadTaskRunnerHandle>>::Leaky
    thread_task_runner_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const scoped_refptr<SingleThreadTaskRunner>& ThreadTaskRunnerHandle::Get() {
  ThreadTaskRunnerHandle* current = thread_task_runner_tls.Pointer()->Get();
  return current->task_runner_;
}
}
