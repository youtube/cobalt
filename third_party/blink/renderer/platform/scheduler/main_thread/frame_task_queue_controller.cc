// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;
using QueueTraits = MainThreadTaskQueue::QueueTraits;
using QueueEnabledVoter = base::sequence_manager::TaskQueue::QueueEnabledVoter;

FrameTaskQueueController::FrameTaskQueueController(
    MainThreadSchedulerImpl* main_thread_scheduler_impl,
    FrameSchedulerImpl* frame_scheduler_impl,
    Delegate* delegate)
    : main_thread_scheduler_impl_(main_thread_scheduler_impl),
      frame_scheduler_impl_(frame_scheduler_impl),
      delegate_(delegate) {
  DCHECK(frame_scheduler_impl_);
  DCHECK(delegate_);
}

FrameTaskQueueController::~FrameTaskQueueController() = default;

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::GetTaskQueue(
    MainThreadTaskQueue::QueueTraits queue_traits) {
  if (!task_queues_.Contains(queue_traits.Key()))
    CreateTaskQueue(queue_traits);
  auto it = task_queues_.find(queue_traits.Key());
  DCHECK(it != task_queues_.end());
  return it->value;
}

const Vector<FrameTaskQueueController::TaskQueueAndEnabledVoterPair>&
FrameTaskQueueController::GetAllTaskQueuesAndVoters() const {
  return all_task_queues_and_voters_;
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::NewWebSchedulingTaskQueue(
    QueueTraits queue_traits,
    WebSchedulingQueueType queue_type,
    WebSchedulingPriority priority) {
  // Note: we only track this |task_queue| in |all_task_queues_and_voters_|.
  // It's interacted with through the MainThreadWebSchedulingTaskQueueImpl that
  // will wrap it, rather than through this class like other task queues.
  scoped_refptr<MainThreadTaskQueue> task_queue =
      main_thread_scheduler_impl_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kWebScheduling)
              .SetQueueTraits(queue_traits)
              .SetWebSchedulingQueueType(queue_type)
              .SetWebSchedulingPriority(priority)
              .SetFrameScheduler(frame_scheduler_impl_));
  TaskQueueCreated(task_queue);
  return task_queue;
}

void FrameTaskQueueController::RemoveWebSchedulingTaskQueue(
    MainThreadTaskQueue* queue) {
  DCHECK(queue);
  RemoveTaskQueueAndVoter(queue);
}

void FrameTaskQueueController::CreateTaskQueue(
    QueueTraits queue_traits) {
  DCHECK(!task_queues_.Contains(queue_traits.Key()));
  // |main_thread_scheduler_impl_| can be null in unit tests.
  DCHECK(main_thread_scheduler_impl_);

  MainThreadTaskQueue::QueueCreationParams queue_creation_params(
      QueueTypeFromQueueTraits(queue_traits));

  queue_creation_params =
      queue_creation_params
          .SetQueueTraits(queue_traits)
          .SetFrameScheduler(frame_scheduler_impl_);

  scoped_refptr<MainThreadTaskQueue> task_queue =
      main_thread_scheduler_impl_->NewTaskQueue(queue_creation_params);
  TaskQueueCreated(task_queue);
  task_queues_.insert(queue_traits.Key(), task_queue);
}

void FrameTaskQueueController::TaskQueueCreated(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  DCHECK(task_queue);

  std::unique_ptr<QueueEnabledVoter> voter =
      task_queue->CreateQueueEnabledVoter();

  delegate_->OnTaskQueueCreated(task_queue.get(), voter.get());

  all_task_queues_and_voters_.push_back(
      TaskQueueAndEnabledVoterPair(task_queue.get(), voter.get()));

  DCHECK(task_queue_enabled_voters_.find(task_queue) ==
         task_queue_enabled_voters_.end());
  task_queue_enabled_voters_.insert(task_queue, std::move(voter));
}

void FrameTaskQueueController::RemoveTaskQueueAndVoter(
    MainThreadTaskQueue* queue) {
  DCHECK(task_queue_enabled_voters_.Contains(queue));
  task_queue_enabled_voters_.erase(queue);

  bool found_task_queue = false;
  for (auto* it = all_task_queues_and_voters_.begin();
       it != all_task_queues_and_voters_.end(); ++it) {
    if (it->first == queue) {
      found_task_queue = true;
      all_task_queues_and_voters_.erase(it);
      break;
    }
  }
  DCHECK(found_task_queue);
}

base::sequence_manager::TaskQueue::QueueEnabledVoter*
FrameTaskQueueController::GetQueueEnabledVoter(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  auto it = task_queue_enabled_voters_.find(task_queue);
  if (it == task_queue_enabled_voters_.end())
    return nullptr;
  return it->value.get();
}

void FrameTaskQueueController::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("task_queues", task_queues_.Values());
}

// static
MainThreadTaskQueue::QueueType
FrameTaskQueueController::QueueTypeFromQueueTraits(QueueTraits queue_traits) {
  // Order matters here, the priority decisions need to be at the top since
  // loading/loading control TQs set some of these other bits.
  if (queue_traits.prioritisation_type ==
      QueueTraits::PrioritisationType::kLoading)
    return MainThreadTaskQueue::QueueType::kFrameLoading;
  if (queue_traits.prioritisation_type ==
      QueueTraits::PrioritisationType::kLoadingControl)
    return MainThreadTaskQueue::QueueType::kFrameLoadingControl;
  if (queue_traits.can_be_throttled)
    return MainThreadTaskQueue::QueueType::kFrameThrottleable;
  if (queue_traits.can_be_deferred)
    return MainThreadTaskQueue::QueueType::kFrameDeferrable;
  if (queue_traits.can_be_paused)
    return MainThreadTaskQueue::QueueType::kFramePausable;
  return MainThreadTaskQueue::QueueType::kFrameUnpausable;
}

}  // namespace scheduler
}  // namespace blink
