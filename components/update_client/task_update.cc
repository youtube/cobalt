// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/update_client/task_update.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskUpdate::TaskUpdate(scoped_refptr<UpdateEngine> update_engine,
                       bool is_foreground,
                       const std::vector<std::string>& ids,
                       UpdateClient::CrxDataCallback crx_data_callback,
                       Callback callback)
    : update_engine_(update_engine),
      is_foreground_(is_foreground),
      ids_(ids),
      crx_data_callback_(std::move(crx_data_callback)),
      callback_(std::move(callback))
#if defined(STARBOARD)
      , is_completed_(false)
#endif
{
#if defined(STARBOARD)
    LOG(INFO) << "TaskUpdate::TaskUpdate";
#endif
}

TaskUpdate::~TaskUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(STARBOARD)
  LOG(INFO) << "TaskUpdate::~TaskUpdate";
#endif
}

void TaskUpdate::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(STARBOARD)
  LOG(INFO) << "TaskUpdate::Run begin";
  if(is_completed_) {
    LOG(WARNING) << "TaskUpdate::Run already completed";
    return;
  }
#endif

  if (ids_.empty()) {
    TaskComplete(Error::INVALID_ARGUMENT);
    return;
  }

#if defined(STARBOARD)
  update_engine_->Update(is_foreground_, ids_, std::move(crx_data_callback_),
                         base::BindOnce(&TaskUpdate::TaskComplete, this),
                         cancelation_closure_);
  LOG(INFO) << "TaskUpdate::Run end";
#else
  update_engine_->Update(is_foreground_, ids_, std::move(crx_data_callback_),
                         base::BindOnce(&TaskUpdate::TaskComplete, this));
#endif

}

void TaskUpdate::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if defined(STARBOARD)
  LOG(INFO) << "TaskUpdate::Cancel";
  if (cancelation_closure_) {  // The engine's picked up the task.
    std::move(cancelation_closure_).Run();
  }
#endif

  TaskComplete(Error::UPDATE_CANCELED);
}

std::vector<std::string> TaskUpdate::GetIds() const {
  return ids_;
}

void TaskUpdate::TaskComplete(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(STARBOARD)
  LOG(INFO) << "TaskUpdate::TaskComplete";

  // The callback is defined as OnceCallback and should not
  // be called multiple times.
  if(is_completed_) {
    LOG(INFO) << "TaskUpdate::TaskComplete already called";
    return;
  }

  is_completed_ = true;
#endif

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                scoped_refptr<TaskUpdate>(this), error));
}

}  // namespace update_client
