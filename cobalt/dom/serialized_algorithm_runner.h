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
class SerializedAlgorithmRunner {
 public:
  // Abstract the non-template part of the Handle class, and shouldn't be used
  // directly.
  class HandleBase : public base::RefCountedThreadSafe<HandleBase> {
   public:
    virtual ~HandleBase() {}

    virtual void Process(bool* finished) = 0;
    virtual void Finalize() = 0;
  };

  // A handle object for a running algorithm instance, to allow for aborting and
  // access the algorithm.
  template <typename SerializedAlgorithm>
  class Handle : public HandleBase {
   public:
    explicit Handle(std::unique_ptr<SerializedAlgorithm> algorithm);

    // Abort the algorithm and no more processing will happen on return.  It is
    // possible that Process() has already finished asynchronously, in which
    // case this function will call Finalize() instead (if it hasn't been called
    // yet).
    void Abort();
    SerializedAlgorithm* algorithm() const { return algorithm_.get(); }

   private:
    void Process(bool* finished) override;
    void Finalize() override;

    // The |mutex_| is necessary as `Abort()` can be called from any thread.
    starboard::Mutex mutex_;
    std::unique_ptr<SerializedAlgorithm> algorithm_;
    bool aborted_ = false;
    bool finished_ = false;
    bool finalized_ = false;
  };

  virtual ~SerializedAlgorithmRunner() {}

  virtual void Start(const scoped_refptr<HandleBase>& handle) = 0;
};

template <typename SerializedAlgorithm>
SerializedAlgorithmRunner::Handle<SerializedAlgorithm>::Handle(
    std::unique_ptr<SerializedAlgorithm> algorithm)
    : algorithm_(std::move(algorithm)) {
  DCHECK(algorithm_);
}

template <typename SerializedAlgorithm>
void SerializedAlgorithmRunner::Handle<SerializedAlgorithm>::Abort() {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Abort()");

  starboard::ScopedLock scoped_lock(mutex_);

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
void SerializedAlgorithmRunner::Handle<SerializedAlgorithm>::Process(
    bool* finished) {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Process()");

  DCHECK(finished);

  starboard::ScopedLock scoped_lock(mutex_);

  DCHECK(!finished_);
  DCHECK(!finalized_);

  if (aborted_) {
    return;
  }

  DCHECK(algorithm_);
  algorithm_->Process(&finished_);
  *finished = finished_;
}

template <typename SerializedAlgorithm>
void SerializedAlgorithmRunner::Handle<SerializedAlgorithm>::Finalize() {
  TRACE_EVENT0("cobalt::dom", "SerializedAlgorithmRunner::Handle::Finalize()");
  starboard::ScopedLock scoped_lock(mutex_);

  DCHECK(finished_);
  DCHECK(!finalized_);

  if (aborted_) {
    return;
  }

  DCHECK(algorithm_);

  algorithm_->Finalize();
  algorithm_.reset();
  finalized_ = true;
}

// This class runs algorithm on the task runner associated with the thread where
// Start() is called.
class DefaultAlgorithmRunner : public SerializedAlgorithmRunner {
 public:
  explicit DefaultAlgorithmRunner(bool asynchronous_reduction_enabled)
      : asynchronous_reduction_enabled_(asynchronous_reduction_enabled) {}

 private:
  void Start(const scoped_refptr<HandleBase>& handle) override;
  void Process(const scoped_refptr<HandleBase>& handle);

  const bool asynchronous_reduction_enabled_;
};

// This class runs algorithms on two task runners, it can be used to offload
// processing to another task runner.
//
// This class will keep calling the Process() member function of the algorithm
// on the process task runner, and then call Finalize() on the finalize task
// runner when |finished| becomes true.
class OffloadAlgorithmRunner : public SerializedAlgorithmRunner {
 public:
  typedef base::SingleThreadTaskRunner TaskRunner;

  OffloadAlgorithmRunner(const scoped_refptr<TaskRunner>& process_task_runner,
                         const scoped_refptr<TaskRunner>& finalize_task_runner);

 private:
  void Start(const scoped_refptr<HandleBase>& handle) override;
  void Process(const scoped_refptr<HandleBase>& handle);

  scoped_refptr<TaskRunner> process_task_runner_;
  scoped_refptr<TaskRunner> finalize_task_runner_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SERIALIZED_ALGORITHM_RUNNER_H_
