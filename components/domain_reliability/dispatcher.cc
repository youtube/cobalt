// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/dispatcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "components/domain_reliability/util.h"

namespace domain_reliability {

struct DomainReliabilityDispatcher::Task {
  Task(base::OnceClosure closure,
       std::unique_ptr<MockableTime::Timer> timer,
       base::TimeDelta min_delay,
       base::TimeDelta max_delay);
  Task(Task&& other);
  Task& operator=(Task&& other);
  ~Task();

  base::OnceClosure closure;
  std::unique_ptr<MockableTime::Timer> timer;
  base::TimeDelta min_delay;
  base::TimeDelta max_delay;
  bool eligible;
};

DomainReliabilityDispatcher::Task::Task(
    base::OnceClosure closure,
    std::unique_ptr<MockableTime::Timer> timer,
    base::TimeDelta min_delay,
    base::TimeDelta max_delay)
    : closure(std::move(closure)),
      timer(std::move(timer)),
      min_delay(min_delay),
      max_delay(max_delay),
      eligible(false) {}

DomainReliabilityDispatcher::Task::Task(Task&& other) = default;

DomainReliabilityDispatcher::Task& DomainReliabilityDispatcher::Task::operator=(
    Task&& other) = default;

DomainReliabilityDispatcher::Task::~Task() = default;

DomainReliabilityDispatcher::DomainReliabilityDispatcher(MockableTime* time)
    : time_(time) {}

DomainReliabilityDispatcher::~DomainReliabilityDispatcher() = default;

void DomainReliabilityDispatcher::ScheduleTask(base::OnceClosure closure,
                                               base::TimeDelta min_delay,
                                               base::TimeDelta max_delay) {
  DCHECK(closure);
  // Would be DCHECK_LE, but you can't << a TimeDelta.
  DCHECK(min_delay <= max_delay);

  std::unique_ptr<Task> owned_task = std::make_unique<Task>(
      std::move(closure), time_->CreateTimer(), min_delay, max_delay);
  Task* task = owned_task.get();
  tasks_.insert(std::move(owned_task));
  if (max_delay.InMicroseconds() < 0)
    RunAndDeleteTask(task);
  else if (min_delay.InMicroseconds() < 0)
    MakeTaskEligible(task);
  else
    MakeTaskWaiting(task);
}

void DomainReliabilityDispatcher::RunEligibleTasks() {
  // Move all eligible tasks to a separate set so that eligible_tasks_.erase in
  // RunAndDeleteTask won't erase elements out from under the iterator.  (Also
  // keeps RunEligibleTasks from running forever if a task adds a new, already-
  // eligible task that does the same, and so on.)
  std::set<Task*> tasks;
  tasks.swap(eligible_tasks_);

  for (auto* task : tasks) {
    DCHECK(task);
    DCHECK(task->eligible);
    RunAndDeleteTask(task);
  }
}

void DomainReliabilityDispatcher::RunAllTasksForTesting() {
  std::set<Task*> tasks;
  for (auto& task : tasks_)
    tasks.insert(task.get());

  for (auto* task : tasks) {
    DCHECK(task);
    RunAndDeleteTask(task);
  }
}

void DomainReliabilityDispatcher::MakeTaskWaiting(Task* task) {
  DCHECK(task);
  DCHECK(!task->eligible);
  DCHECK(!task->timer->IsRunning());
  task->timer->Start(
      FROM_HERE, task->min_delay,
      base::BindOnce(&DomainReliabilityDispatcher::MakeTaskEligible,
                     base::Unretained(this), task));
}

void
DomainReliabilityDispatcher::MakeTaskEligible(Task* task) {
  DCHECK(task);
  DCHECK(!task->eligible);
  task->eligible = true;
  eligible_tasks_.insert(task);
  task->timer->Start(
      FROM_HERE, task->max_delay - task->min_delay,
      base::BindOnce(&DomainReliabilityDispatcher::RunAndDeleteTask,
                     base::Unretained(this), task));
}

void DomainReliabilityDispatcher::RunAndDeleteTask(Task* task) {
  DCHECK(task);
  DCHECK(task->closure);
  std::move(task->closure).Run();
  if (task->eligible)
    eligible_tasks_.erase(task);

  auto it = tasks_.find(task);
  DCHECK(it != tasks_.end());
  tasks_.erase(it);
}

}  // namespace domain_reliability
