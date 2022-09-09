// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_DOM_SERIALIZED_ALGORITHM_RUNNER_H_
#define COBALT_DOM_SERIALIZED_ALGORITHM_RUNNER_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "starboard/common/mutex.h"

namespace cobalt {
namespace dom {

// This class defines a common interface to run algorithms, and an algorithm is
// any class with the following methods:
//   void Process(bool* finished);
//   void Abort();
//   void Finalize();
//
// For example, with a class:
// class Download {
//   void Process(bool* finished) {
//     // Download some data
//     // *finished = whether download is finished.
//   }
//   void Abort() {
//     // Cancel the download, and neither Process() nor Finalize() will be
//     // called again.
//   }
//   void Finalize() {
//     // Queue an event to notify that the download has finished.
//   }
// };
//
// This class will keep calling Process() until |finished| becomes false, and
// then call Finalize().  It guarantees that all calls won't be overlapped and
// the member functions of the algorithm don't have to synchronize between them.
template <typename SerializedAlgorithm>
class SerializedAlgorithmRunner {
 public:
  // A handle object for a running algorithm instance, to allow for aborting and
  // access the algorithm.
  class Handle : public base::RefCountedThreadSafe<Handle> {
   public:
    // Abort the algorithm and no more processing will happen on return.  It is
    // possible that Process() has already finished asynchronously, in which
    // case this function will call Finalize() instead (if it hasn't been called
    // yet).
    void Abort();
    void Process(bool* finished);
    void FinalizeIfNotAborted();

    SerializedAlgorithm* algorithm() const { return algorithm_.get(); }

   private:
    friend class SerializedAlgorithmRunner;

    // Provide synchronization only when |synchronization_required| is true.
    // This allows bypassing of synchronization for algorithm runners that
    // operate on a single thread, where a mutex could be reentrant if acquired
    // due to nested calls.
    class ScopedLockWhenRequired {
     public:
      ScopedLockWhenRequired(bool synchronization_required,
                             const starboard::Mutex& mutex)
          : synchronization_required_(synchronization_required), mutex_(mutex) {
        if (synchronization_required_) {
          mutex_.Acquire();
        }
      }
      ~ScopedLockWhenRequired() {
        if (synchronization_required_) {
          mutex_.Release();
        }
      }

     private:
      const bool synchronization_required_;
      const starboard::Mutex& mutex_;
    };

    Handle(bool synchronization_required,
           std::unique_ptr<SerializedAlgorithm> algorithm);

    // The |mutex_| is necessary for algorithm runners operate on multiple
    // threads as `Abort()` can be called from any thread.
    const bool synchronization_required_;
    starboard::Mutex mutex_;
    std::unique_ptr<SerializedAlgorithm> algorithm_;
    bool aborted_ = false;
    bool finished_ = false;
    bool finalized_ = false;
  };

  virtual ~SerializedAlgorithmRunner() {}

  virtual scoped_refptr<Handle> CreateHandle(
      std::unique_ptr<SerializedAlgorithm> algorithm) = 0;
  virtual void Start(scoped_refptr<Handle> handle) = 0;

 protected:
  scoped_refptr<Handle> CreateHandle(
      bool synchronization_required,
      std::unique_ptr<SerializedAlgorithm> algorithm) {
    return new Handle(synchronization_required, std::move(algorithm));
  }
};

template <typename SerializedAlgorithm>
SerializedAlgorithmRunner<SerializedAlgorithm>::Handle::Handle(
    bool synchronization_required,
    std::unique_ptr<SerializedAlgorithm> algorithm)
    : synchronization_required_(synchronization_required),
      algorithm_(std::move(algorithm)) {
  DCHECK(algorithm_);
}

template <typename SerializedAlgorithm>
void SerializedAlgorithmRunner<SerializedAlgorithm>::Handle::Abort() {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Abort()");

  ScopedLockWhenRequired scoped_lock(synchronization_required_, mutex_);

  DCHECK(!aborted_);  // Abort() cannot be called twice.

  if (finished_) {
    // If the algorithm has finished, just call Finalize() to treat it as
    // finished instead of aborted.
    if (!finalized_) {
      algorithm_->Finalize();
    }
  } else {
    algorithm_->Abort();
  }

  algorithm_.reset();
  aborted_ = true;
}

template <typename SerializedAlgorithm>
void SerializedAlgorithmRunner<SerializedAlgorithm>::Handle::Process(
    bool* finished) {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Process()");

  DCHECK(finished);

  ScopedLockWhenRequired scoped_lock(synchronization_required_, mutex_);

  DCHECK(!finished_);
  DCHECK(!finalized_);

  if (aborted_) {
    *finished = true;
    return;
  }

  DCHECK(algorithm_);
  algorithm_->Process(&finished_);
  *finished = finished_;
}

template <typename SerializedAlgorithm>
void SerializedAlgorithmRunner<
    SerializedAlgorithm>::Handle::FinalizeIfNotAborted() {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Finalize()");

  ScopedLockWhenRequired scoped_lock(synchronization_required_, mutex_);

  DCHECK(!finalized_);

  if (aborted_) {
    return;
  }

  DCHECK(finished_);
  DCHECK(algorithm_);

  algorithm_->Finalize();
  algorithm_.reset();
  finalized_ = true;
}

// This class runs algorithm on the task runner associated with the thread where
// Start() is called.
template <typename SerializedAlgorithm>
class DefaultAlgorithmRunner
    : public SerializedAlgorithmRunner<SerializedAlgorithm> {
 public:
  explicit DefaultAlgorithmRunner(bool asynchronous_reduction_enabled)
      : asynchronous_reduction_enabled_(asynchronous_reduction_enabled) {}

 private:
  typedef
      typename SerializedAlgorithmRunner<SerializedAlgorithm>::Handle Handle;

  scoped_refptr<Handle> CreateHandle(
      std::unique_ptr<SerializedAlgorithm> algorithm) override;
  void Start(scoped_refptr<Handle> handle) override;
  void Process(scoped_refptr<Handle> handle);

  const bool asynchronous_reduction_enabled_;
};

// This class runs algorithms on two task runners, it can be used to offload
// processing to another task runner.
//
// This class will keep calling the Process() member function of the algorithm
// on the process task runner, and then call Finalize() on the finalize task
// runner when |finished| becomes true.
template <typename SerializedAlgorithm>
class OffloadAlgorithmRunner
    : public SerializedAlgorithmRunner<SerializedAlgorithm> {
 public:
  typedef base::SingleThreadTaskRunner TaskRunner;

  OffloadAlgorithmRunner(const scoped_refptr<TaskRunner>& process_task_runner,
                         const scoped_refptr<TaskRunner>& finalize_task_runner);

 private:
  typedef
      typename SerializedAlgorithmRunner<SerializedAlgorithm>::Handle Handle;

  scoped_refptr<Handle> CreateHandle(
      std::unique_ptr<SerializedAlgorithm> algorithm) override;
  void Start(scoped_refptr<Handle> handle) override;
  void Process(scoped_refptr<Handle> handle);

  scoped_refptr<TaskRunner> process_task_runner_;
  scoped_refptr<TaskRunner> finalize_task_runner_;
};

template <typename SerializedAlgorithm>
scoped_refptr<typename SerializedAlgorithmRunner<SerializedAlgorithm>::Handle>
DefaultAlgorithmRunner<SerializedAlgorithm>::CreateHandle(
    std::unique_ptr<SerializedAlgorithm> algorithm) {
  TRACE_EVENT0("cobalt::dom", "DefaultAlgorithmRunner::CreateHandle()");

  const bool kSynchronizationRequired = false;
  return SerializedAlgorithmRunner<SerializedAlgorithm>::CreateHandle(
      kSynchronizationRequired, std::move(algorithm));
}

template <typename SerializedAlgorithm>
void DefaultAlgorithmRunner<SerializedAlgorithm>::Start(
    scoped_refptr<Handle> handle) {
  DCHECK(handle);
  TRACE_EVENT0("cobalt::dom", "DefaultAlgorithmRunner::Start()");

  if (asynchronous_reduction_enabled_) {
    Process(handle);
    return;
  }

  auto task_runner = base::MessageLoop::current()->task_runner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DefaultAlgorithmRunner::Process,
                                       base::Unretained(this), handle));
}

template <typename SerializedAlgorithm>
void DefaultAlgorithmRunner<SerializedAlgorithm>::Process(
    scoped_refptr<Handle> handle) {
  DCHECK(handle);
  TRACE_EVENT0("cobalt::dom", "DefaultAlgorithmRunner::Process()");

  auto task_runner = base::MessageLoop::current()->task_runner();

  bool finished = false;
  handle->Process(&finished);

  if (finished) {
    handle->FinalizeIfNotAborted();
    return;
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DefaultAlgorithmRunner::Process,
                                       base::Unretained(this), handle));
}

template <typename SerializedAlgorithm>
OffloadAlgorithmRunner<SerializedAlgorithm>::OffloadAlgorithmRunner(
    const scoped_refptr<TaskRunner>& process_task_runner,
    const scoped_refptr<TaskRunner>& finalize_task_runner)
    : process_task_runner_(process_task_runner),
      finalize_task_runner_(finalize_task_runner) {
  DCHECK(process_task_runner_);
  DCHECK(finalize_task_runner_);
  DCHECK_NE(process_task_runner_, finalize_task_runner_);
}

template <typename SerializedAlgorithm>
scoped_refptr<typename SerializedAlgorithmRunner<SerializedAlgorithm>::Handle>
OffloadAlgorithmRunner<SerializedAlgorithm>::CreateHandle(
    std::unique_ptr<SerializedAlgorithm> algorithm) {
  TRACE_EVENT0("cobalt::dom", "OffloadAlgorithmRunner::CreateHandle()");

  const bool kSynchronizationRequired = true;
  return SerializedAlgorithmRunner<SerializedAlgorithm>::CreateHandle(
      kSynchronizationRequired, std::move(algorithm));
}

template <typename SerializedAlgorithm>
void OffloadAlgorithmRunner<SerializedAlgorithm>::Start(
    scoped_refptr<Handle> handle) {
  DCHECK(handle);

  TRACE_EVENT0("cobalt::dom", "OffloadAlgorithmRunner::Start()");

  process_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OffloadAlgorithmRunner::Process,
                                base::Unretained(this), handle));
}

template <typename SerializedAlgorithm>
void OffloadAlgorithmRunner<SerializedAlgorithm>::Process(
    scoped_refptr<Handle> handle) {
  DCHECK(handle);
  DCHECK(process_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT0("cobalt::dom", "OffloadAlgorithmRunner::Process()");

  bool finished = false;
  handle->Process(&finished);

  if (finished) {
    finalize_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Handle::FinalizeIfNotAborted, handle));
    return;
  }
  process_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OffloadAlgorithmRunner::Process,
                                base::Unretained(this), handle));
}

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SERIALIZED_ALGORITHM_RUNNER_H_
