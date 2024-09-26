// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_task_queue.h"

#include <map>
#include <utility>

#include "components/webapps/browser/installable/installable_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

InstallableTask::InstallableTask() = default;

InstallableTask::InstallableTask(const InstallableParams& params,
                                 InstallableCallback callback)
    : params(params), callback(std::move(callback)) {}

InstallableTask::~InstallableTask() = default;

InstallableTask::InstallableTask(InstallableTask&& other) = default;

InstallableTask& InstallableTask::operator=(InstallableTask&& other) = default;

InstallableTaskQueue::InstallableTaskQueue() = default;

InstallableTaskQueue::~InstallableTaskQueue() = default;

void InstallableTaskQueue::Add(InstallableTask task) {
  tasks_.push_back(std::move(task));
}

void InstallableTaskQueue::PauseCurrent() {
  DCHECK(HasCurrent());
  paused_tasks_.push_back(std::move(Current()));
  Next();
}

void InstallableTaskQueue::UnpauseAll() {
  while (!paused_tasks_.empty()) {
    Add(std::move(paused_tasks_.front()));
    paused_tasks_.pop_front();
  }
}

bool InstallableTaskQueue::HasCurrent() const {
  return !tasks_.empty();
}

bool InstallableTaskQueue::HasPaused() const {
  return !paused_tasks_.empty();
}

InstallableTask& InstallableTaskQueue::Current() {
  DCHECK(HasCurrent());
  return tasks_.front();
}

void InstallableTaskQueue::Next() {
  DCHECK(HasCurrent());
  tasks_.pop_front();
}

void InstallableTaskQueue::ResetWithError(InstallableStatusCode code) {
  std::deque<InstallableTask> tasks = std::move(tasks_);
  std::deque<InstallableTask> paused_tasks = std::move(paused_tasks_);
  // Some callbacks might be already invalidated on certain resets, so we must
  // check for that.
  // Manifest is assumed to be non-null, so we create an empty one here.
  blink::mojom::Manifest manifest;
  for (InstallableTask& task : tasks) {
    if (task.callback) {
      std::move(task.callback)
          .Run(InstallableData({code}, GURL(), manifest, GURL(), nullptr, false,
                               GURL(), nullptr, false,
                               std::vector<Screenshot>(), false, false));
    }
  }
  for (InstallableTask& task : paused_tasks) {
    if (task.callback) {
      std::move(task.callback)
          .Run(InstallableData({code}, GURL(), manifest, GURL(), nullptr, false,
                               GURL(), nullptr, false,
                               std::vector<Screenshot>(), false, false));
    }
  }
}

}  // namespace webapps
