//    Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequenced_worker_pool.h"

#include <deque>
#include <set>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"

namespace base {

namespace {

struct SequencedTask {
  int sequence_token_id;
  SequencedWorkerPool::WorkerShutdown shutdown_behavior;
  tracked_objects::Location location;
  base::Closure task;
};

}  // namespace

// Worker ---------------------------------------------------------------------

class SequencedWorkerPool::Worker : public base::SimpleThread {
 public:
  Worker(SequencedWorkerPool::Inner* inner,
         int thread_number,
         const std::string& thread_name_prefix);
  ~Worker();

  // SimpleThread implementation. This actually runs the background thread.
  virtual void Run();

 private:
  SequencedWorkerPool::Inner* inner_;
  SequencedWorkerPool::WorkerShutdown current_shutdown_mode_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};


// Inner ----------------------------------------------------------------------

class SequencedWorkerPool::Inner
    : public base::RefCountedThreadSafe<SequencedWorkerPool::Inner> {
 public:
  Inner(size_t max_threads, const std::string& thread_name_prefix);
  virtual ~Inner();

  // Backends for SequenceWorkerPool.
  SequenceToken GetSequenceToken();
  SequenceToken GetNamedSequenceToken(const std::string& name);
  bool PostTask(int sequence_token_id,
                SequencedWorkerPool::WorkerShutdown shutdown_behavior,
                const tracked_objects::Location& from_here,
                const base::Closure& task);
  void Shutdown();
  void SetTestingObserver(SequencedWorkerPool::TestingObserver* observer);

  // Runs the worker loop on the background thread.
  void ThreadLoop(Worker* this_worker);

 private:
  // The calling code should clear the given delete_these_oustide_lock
  // vector the next time the lock is released. See the implementation for
  // a more detailed description.
  bool GetWork(SequencedTask* task,
               std::vector<base::Closure>* delete_these_outside_lock);

  // Peforms init and cleanup around running the given task. WillRun...
  // returns the value from PrepareToStartAdditionalThreadIfNecessary.
  // The calling code should call FinishStartingAdditionalThread once the
  // lock is released if the return values is nonzero.
  int WillRunWorkerTask(const SequencedTask& task);
  void DidRunWorkerTask(const SequencedTask& task);

  // Returns true if there are no threads currently running the given
  // sequence token.
  bool IsSequenceTokenRunnable(int sequence_token_id) const;

  // Checks if all threads are busy and the addition of one more could run an
  // additional task waiting in the queue. This must be called from within
  // the lock.
  //
  // If another thread is helpful, this will mark the thread as being in the
  // process of starting and returns the index of the new thread which will be
  // 0 or more. The caller should then call FinishStartingAdditionalThread to
  // complete initialization once the lock is released.
  //
  // If another thread is not necessary, returne 0;
  //
  // See the implementedion for more.
  int PrepareToStartAdditionalThreadIfHelpful();

  // The second part of thread creation after
  // PrepareToStartAdditionalThreadIfHelpful with the thread number it
  // generated. This actually creates the thread and should be called outside
  // the lock to avoid blocking important work starting a thread in the lock.
  void FinishStartingAdditionalThread(int thread_number);

  // Checks whether there is work left that's blocking shutdown. Must be
  // called inside the lock.
  bool CanShutdown() const;

  // The last sequence number used. Managed by GetSequenceToken, since this
  // only does threadsafe increment operations, you do not need to hold the
  // lock.
  volatile base::subtle::Atomic32 last_sequence_number_;

  // This lock protects |everything in this class|. Do not read or modify
  // anything without holding this lock. Do not block while holding this
  // lock.
  base::Lock lock_;

  // Condition variable used to wake up worker threads when a task is runnable.
  base::ConditionVariable cond_var_;

  // The maximum number of worker threads we'll create.
  size_t max_threads_;

  std::string thread_name_prefix_;

  // Associates all known sequence token names with their IDs.
  std::map<std::string, int> named_sequence_tokens_;

  // Owning pointers to all threads we've created so far. Since we lazily
  // create threads, this may be less than max_threads_ and will be initially
  // empty.
  std::vector<linked_ptr<Worker> > threads_;

  // Set to true when we're in the process of creating another thread.
  // See PrepareToStartAdditionalThreadIfHelpful for more.
  bool thread_being_created_;

  // Number of threads currently waiting for work.
  size_t waiting_thread_count_;

  // Number of threads currently running tasks that have the BLOCK_SHUTDOWN
  // flag set.
  size_t blocking_shutdown_thread_count_;

  // In-order list of all pending tasks. These are tasks waiting for a thread
  // to run on or that are blocked on a previous task in their sequence.
  //
  // We maintain the pending_task_count_ separately for metrics because
  // list.size() can be linear time.
  std::list<SequencedTask> pending_tasks_;
  size_t pending_task_count_;

  // Number of tasks in the pending_tasks_ list that are marked as blocking
  // shutdown.
  size_t blocking_shutdown_pending_task_count_;

  // Lists all sequence tokens currently executing.
  std::set<int> current_sequences_;

  // Set when the app is terminating and no further tasks should be allowed,
  // though we may still be running existing tasks.
  bool terminating_;

  // Set when Shutdown is called to do some assertions.
  bool shutdown_called_;

  SequencedWorkerPool::TestingObserver* testing_observer_;
};

SequencedWorkerPool::Worker::Worker(SequencedWorkerPool::Inner* inner,
                                    int thread_number,
                                    const std::string& prefix)
    : base::SimpleThread(
          prefix + StringPrintf("Worker%d", thread_number).c_str()),
      inner_(inner),
      current_shutdown_mode_(SequencedWorkerPool::CONTINUE_ON_SHUTDOWN) {
  Start();
}

SequencedWorkerPool::Worker::~Worker() {
}

void SequencedWorkerPool::Worker::Run() {
  // Just jump back to the Inner object to run the thread, since it has all the
  // tracking information and queues. It might be more natural to implement
  // using DelegateSimpleThread and have Inner implement the Delegate to avoid
  // having these worker objects at all, but that method lacks the ability to
  // send thread-specific information easily to the thread loop.
  inner_->ThreadLoop(this);
}

SequencedWorkerPool::Inner::Inner(size_t max_threads,
                                  const std::string& thread_name_prefix)
    : last_sequence_number_(0),
      lock_(),
      cond_var_(&lock_),
      max_threads_(max_threads),
      thread_name_prefix_(thread_name_prefix),
      thread_being_created_(false),
      waiting_thread_count_(0),
      blocking_shutdown_thread_count_(0),
      pending_task_count_(0),
      blocking_shutdown_pending_task_count_(0),
      terminating_(false),
      shutdown_called_(false) {
}

SequencedWorkerPool::Inner::~Inner() {
  // You must call Shutdown() before destroying the pool.
  DCHECK(shutdown_called_);

  // Need to explicitly join with the threads before they're destroyed or else
  // they will be running when our object is half torn down.
  for (size_t i = 0; i < threads_.size(); i++)
    threads_[i]->Join();
  threads_.clear();
}

SequencedWorkerPool::SequenceToken
SequencedWorkerPool::Inner::GetSequenceToken() {
  base::subtle::Atomic32 result =
      base::subtle::NoBarrier_AtomicIncrement(&last_sequence_number_, 1);
  return SequenceToken(static_cast<int>(result));
}

SequencedWorkerPool::SequenceToken
SequencedWorkerPool::Inner::GetNamedSequenceToken(
    const std::string& name) {
  base::AutoLock lock(lock_);
  std::map<std::string, int>::const_iterator found =
      named_sequence_tokens_.find(name);
  if (found != named_sequence_tokens_.end())
    return SequenceToken(found->second);  // Got an existing one.

  // Create a new one for this name.
  SequenceToken result = GetSequenceToken();
  named_sequence_tokens_.insert(std::make_pair(name, result.id_));
  return result;
}

bool SequencedWorkerPool::Inner::PostTask(
    int sequence_token_id,
    SequencedWorkerPool::WorkerShutdown shutdown_behavior,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  SequencedTask sequenced;
  sequenced.sequence_token_id = sequence_token_id;
  sequenced.shutdown_behavior = shutdown_behavior;
  sequenced.location = from_here;
  sequenced.task = task;

  int create_thread_id = 0;
  {
    base::AutoLock lock(lock_);
    if (terminating_)
      return false;

    pending_tasks_.push_back(sequenced);
    pending_task_count_++;
    if (shutdown_behavior == BLOCK_SHUTDOWN)
      blocking_shutdown_pending_task_count_++;

    create_thread_id = PrepareToStartAdditionalThreadIfHelpful();
  }

  // Actually start the additional thread or signal an existing one now that
  // we're outside the lock.
  if (create_thread_id)
    FinishStartingAdditionalThread(create_thread_id);
  else
    cond_var_.Signal();

  return true;
}

void SequencedWorkerPool::Inner::Shutdown() {
  if (shutdown_called_)
    return;
  shutdown_called_ = true;

  // Mark us as terminated and go through and drop all tasks that aren't
  // required to run on shutdown. Since no new tasks will get posted once the
  // terminated flag is set, this ensures that all remaining tasks are required
  // for shutdown whenever the termianted_ flag is set.
  {
    base::AutoLock lock(lock_);
    DCHECK(!terminating_);
    terminating_ = true;

    // Tickle the threads. This will wake up a waiting one so it will know that
    // it can exit, which in turn will wake up any other waiting ones.
    cond_var_.Signal();

    // There are no pending or running tasks blocking shutdown, we're done.
    if (CanShutdown())
      return;
  }

  // If we get here, we know we're either waiting on a blocking task that's
  // currently running, waiting on a blocking task that hasn't been scheduled
  // yet, or both. Block on the "queue empty" event to know when all tasks are
  // complete. This must be done outside the lock.
  if (testing_observer_)
    testing_observer_->WillWaitForShutdown();

  base::TimeTicks shutdown_wait_begin = base::TimeTicks::Now();

  // Wait for no more tasks.
  {
    base::AutoLock lock(lock_);
    while (!CanShutdown())
      cond_var_.Wait();
  }
  UMA_HISTOGRAM_TIMES("SequencedWorkerPool.ShutdownDelayTime",
                      base::TimeTicks::Now() - shutdown_wait_begin);
}

void SequencedWorkerPool::Inner::SetTestingObserver(
    SequencedWorkerPool::TestingObserver* observer) {
  base::AutoLock lock(lock_);
  testing_observer_ = observer;
}

void SequencedWorkerPool::Inner::ThreadLoop(Worker* this_worker) {
  {
    base::AutoLock lock(lock_);
    DCHECK(thread_being_created_);
    thread_being_created_ = false;
    threads_.push_back(linked_ptr<Worker>(this_worker));

    while (true) {
      // See GetWork for what delete_these_outside_lock is doing.
      SequencedTask task;
      std::vector<base::Closure> delete_these_outside_lock;
      if (GetWork(&task, &delete_these_outside_lock)) {
        int new_thread_id = WillRunWorkerTask(task);
        {
          base::AutoUnlock unlock(lock_);
          cond_var_.Signal();
          delete_these_outside_lock.clear();

          // Complete thread creation outside the lock if necessary.
          if (new_thread_id)
            FinishStartingAdditionalThread(new_thread_id);

          task.task.Run();

          // Make sure our task is erased outside the lock for the same reason
          // we do this with delete_these_oustide_lock.
          task.task = base::Closure();
        }
        DidRunWorkerTask(task);  // Must be done inside the lock.
      } else {
        // When we're terminating and there's no more work, we can shut down.
        // You can't get more tasks posted once terminating_ is set. There may
        // be some tasks stuck behind running ones with the same sequence
        // token, but additional threads won't help this case.
        if (terminating_)
          break;
        waiting_thread_count_++;
        cond_var_.Wait();
        waiting_thread_count_--;
      }
    }
  }

  // We noticed we should exit. Wake up the next worker so it knows it should
  // exit as well (because the Shutdown() code only signals once).
  cond_var_.Signal();
}

bool SequencedWorkerPool::Inner::GetWork(
    SequencedTask* task,
    std::vector<base::Closure>* delete_these_outside_lock) {
  lock_.AssertAcquired();

  DCHECK_EQ(pending_tasks_.size(), pending_task_count_);
  UMA_HISTOGRAM_COUNTS_100("SequencedWorkerPool.TaskCount",
                           static_cast<int>(pending_task_count_));

  // Find the next task with a sequence token that's not currently in use.
  // If the token is in use, that means another thread is running something
  // in that sequence, and we can't run it without going out-of-order.
  //
  // This algorithm is simple and fair, but inefficient in some cases. For
  // example, say somebody schedules 1000 slow tasks with the same sequence
  // number. We'll have to go through all those tasks each time we feel like
  // there might be work to schedule. If this proves to be a problem, we
  // should make this more efficient.
  //
  // One possible enhancement would be to keep a map from sequence ID to a
  // list of pending but currently blocked SequencedTasks for that ID.
  // When a worker finishes a task of one sequence token, it can pick up the
  // next one from that token right away.
  //
  // This may lead to starvation if there are sufficient numbers of sequences
  // in use. To alleviate this, we could add an incrementing priority counter
  // to each SequencedTask. Then maintain a priority_queue of all runnable
  // tasks, sorted by priority counter. When a sequenced task is completed
  // we would pop the head element off of that tasks pending list and add it
  // to the priority queue. Then we would run the first item in the priority
  // queue.
  bool found_task = false;
  int unrunnable_tasks = 0;
  std::list<SequencedTask>::iterator i = pending_tasks_.begin();
  while (i != pending_tasks_.end()) {
    if (!IsSequenceTokenRunnable(i->sequence_token_id)) {
      unrunnable_tasks++;
      ++i;
      continue;
    }

    if (terminating_ && i->shutdown_behavior != BLOCK_SHUTDOWN) {
      // We're shutting down and the task we just found isn't blocking
      // shutdown. Delete it and get more work.
      //
      // Note that we do not want to delete unrunnable tasks. Deleting a task
      // can have side effects (like freeing some objects) and deleting a
      // task that's supposed to run after one that's currently running could
      // cause an obscure crash.
      //
      // We really want to delete these tasks outside the lock in case the
      // closures are holding refs to objects that want to post work from
      // their destructorss (which would deadlock). The closures are
      // internally refcounted, so we just need to keep a copy of them alive
      // until the lock is exited. The calling code can just clear() the
      // vector they passed to us once the lock is exited to make this
      // happen.
      delete_these_outside_lock->push_back(i->task);
      i = pending_tasks_.erase(i);
      pending_task_count_--;
    } else {
      // Found a runnable task.
      *task = *i;
      i = pending_tasks_.erase(i);
      pending_task_count_--;
      if (task->shutdown_behavior == BLOCK_SHUTDOWN)
        blocking_shutdown_pending_task_count_--;

      found_task = true;
      break;
    }
  }

  // Track the number of tasks we had to skip over to see if we should be
  // making this more efficient. If this number ever becomes large or is
  // frequently "some", we should consider the optimization above.
  UMA_HISTOGRAM_COUNTS_100("SequencedWorkerPool.UnrunnableTaskCount",
                           unrunnable_tasks);
  return found_task;
}

int SequencedWorkerPool::Inner::WillRunWorkerTask(const SequencedTask& task) {
  lock_.AssertAcquired();

  // Mark the task's sequence number as in use.
  if (task.sequence_token_id)
    current_sequences_.insert(task.sequence_token_id);

  if (task.shutdown_behavior == SequencedWorkerPool::BLOCK_SHUTDOWN)
    blocking_shutdown_thread_count_++;

  // We just picked up a task. Since StartAdditionalThreadIfHelpful only
  // creates a new thread if there is no free one, there is a race when posting
  // tasks that many tasks could have been posted before a thread started
  // running them, so only one thread would have been created. So we also check
  // whether we should create more threads after removing our task from the
  // queue, which also has the nice side effect of creating the workers from
  // background threads rather than the main thread of the app.
  //
  // If another thread wasn't created, we want to wake up an existing thread
  // if there is one waiting to pick up the next task.
  //
  // Note that we really need to do this *before* running the task, not
  // after. Otherwise, if more than one task is posted, the creation of the
  // second thread (since we only create one at a time) will be blocked by
  // the execution of the first task, which could be arbitrarily long.
  return PrepareToStartAdditionalThreadIfHelpful();
}

void SequencedWorkerPool::Inner::DidRunWorkerTask(const SequencedTask& task) {
  lock_.AssertAcquired();

  if (task.shutdown_behavior == SequencedWorkerPool::BLOCK_SHUTDOWN) {
    DCHECK_GT(blocking_shutdown_thread_count_, 0u);
    blocking_shutdown_thread_count_--;
  }

  if (task.sequence_token_id)
    current_sequences_.erase(task.sequence_token_id);
}

bool SequencedWorkerPool::Inner::IsSequenceTokenRunnable(
    int sequence_token_id) const {
  lock_.AssertAcquired();
  return !sequence_token_id ||
      current_sequences_.find(sequence_token_id) ==
          current_sequences_.end();
}

int SequencedWorkerPool::Inner::PrepareToStartAdditionalThreadIfHelpful() {
  // How thread creation works:
  //
  // We'de like to avoid creating threads with the lock held. However, we
  // need to be sure that we have an accurate accounting of the threads for
  // proper Joining and deltion on shutdown.
  //
  // We need to figure out if we need another thread with the lock held, which
  // is what this function does. It then marks us as in the process of creating
  // a thread. When we do shutdown, we wait until the thread_being_created_
  // flag is cleared, which ensures that the new thread is properly added to
  // all the data structures and we can't leak it. Once shutdown starts, we'll
  // refuse to create more threads or they would be leaked.
  //
  // Note that this creates a mostly benign race condition on shutdown that
  // will cause fewer workers to be created than one would expect. It isn't
  // much of an issue in real life, but affects some tests. Since we only spawn
  // one worker at a time, the following sequence of events can happen:
  //
  //  1. Main thread posts a bunch of unrelated tasks that would normally be
  //     run on separate threads.
  //  2. The first task post causes us to start a worker. Other tasks do not
  //     cause a worker to start since one is pending.
  //  3. Main thread initiates shutdown.
  //  4. No more threads are created since the terminating_ flag is set.
  //
  // The result is that one may expect that max_threads_ workers to be created
  // given the workload, but in reality fewer may be created because the
  // sequence of thread creation on the background threads is racing with the
  // shutdown call.
  if (!terminating_ &&
      !thread_being_created_ &&
      threads_.size() < max_threads_ &&
      waiting_thread_count_ == 0) {
    // We could use an additional thread if there's work to be done.
    for (std::list<SequencedTask>::iterator i = pending_tasks_.begin();
         i != pending_tasks_.end(); ++i) {
      if (IsSequenceTokenRunnable(i->sequence_token_id)) {
        // Found a runnable task, mark the thread as being started.
        thread_being_created_ = true;
        return static_cast<int>(threads_.size() + 1);
      }
    }
  }
  return 0;
}

void SequencedWorkerPool::Inner::FinishStartingAdditionalThread(
    int thread_number) {
  // Called outside of the lock.
  DCHECK(thread_number > 0);

  // The worker is assigned to the list when the thread actually starts, which
  // will manage the memory of the pointer.
  new Worker(this, thread_number, thread_name_prefix_);
}

bool SequencedWorkerPool::Inner::CanShutdown() const {
  lock_.AssertAcquired();
  // See PrepareToStartAdditionalThreadIfHelpful for how thread creation works.
  return !thread_being_created_ &&
         blocking_shutdown_thread_count_ == 0 &&
         blocking_shutdown_pending_task_count_ == 0;
}

// SequencedWorkerPool --------------------------------------------------------

SequencedWorkerPool::SequencedWorkerPool(size_t max_threads,
                                         const std::string& thread_name_prefix)
    : inner_(new Inner(max_threads, thread_name_prefix)) {
}

SequencedWorkerPool::~SequencedWorkerPool() {
}

SequencedWorkerPool::SequenceToken SequencedWorkerPool::GetSequenceToken() {
  return inner_->GetSequenceToken();
}

SequencedWorkerPool::SequenceToken SequencedWorkerPool::GetNamedSequenceToken(
    const std::string& name) {
  return inner_->GetNamedSequenceToken(name);
}

bool SequencedWorkerPool::PostWorkerTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return inner_->PostTask(0, BLOCK_SHUTDOWN, from_here, task);
}

bool SequencedWorkerPool::PostWorkerTaskWithShutdownBehavior(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    WorkerShutdown shutdown_behavior) {
  return inner_->PostTask(0, shutdown_behavior, from_here, task);
}

bool SequencedWorkerPool::PostSequencedWorkerTask(
    SequenceToken sequence_token,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return inner_->PostTask(sequence_token.id_, BLOCK_SHUTDOWN,
                          from_here, task);
}

bool SequencedWorkerPool::PostSequencedWorkerTaskWithShutdownBehavior(
    SequenceToken sequence_token,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    WorkerShutdown shutdown_behavior) {
  return inner_->PostTask(sequence_token.id_, shutdown_behavior,
                          from_here, task);
}

void SequencedWorkerPool::Shutdown() {
  inner_->Shutdown();
}

void SequencedWorkerPool::SetTestingObserver(TestingObserver* observer) {
  inner_->SetTestingObserver(observer);
}

}  // namespace base
