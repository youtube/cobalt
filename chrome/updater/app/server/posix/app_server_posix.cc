// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/posix/app_server_posix.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/app/server/posix/update_service_stub.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/posix/setup.h"

namespace updater {

AppServerPosix::AppServerPosix() = default;
AppServerPosix::~AppServerPosix() = default;

void AppServerPosix::TaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++tasks_running_;
  VLOG(2) << "Starting task, " << tasks_running_ << " tasks running";
}

void AppServerPosix::TaskCompleted() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppServerPosix::AcknowledgeTaskCompletion, this),
      external_constants()->ServerKeepAliveTime());
}

void AppServerPosix::AcknowledgeTaskCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (--tasks_running_ < 1) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AppServerPosix::Shutdown, this, 0));
  }
  VLOG(2) << "Completing task, " << tasks_running_ << " tasks running";
}

void AppServerPosix::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

void AppServerPosix::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // These delegates need to have a reference to the AppServer. To break the
  // circular reference, we need to reset them.
  active_duty_stub_.reset();
  active_duty_internal_stub_.reset();

  AppServer::Uninitialize();
}

void AppServerPosix::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_duty_internal_stub_ = std::make_unique<UpdateServiceInternalStub>(
      std::move(update_service_internal), updater_scope(),
      base::BindRepeating(&AppServerPosix::TaskStarted, this),
      base::BindRepeating(&AppServerPosix::TaskCompleted, this));
}

void AppServerPosix::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_duty_stub_ = std::make_unique<UpdateServiceStub>(
      std::move(update_service), updater_scope(),
      base::BindRepeating(&AppServerPosix::TaskStarted, this),
      base::BindRepeating(&AppServerPosix::TaskCompleted, this));
}

bool AppServerPosix::SwapInNewVersion() {
  int result = PromoteCandidate(updater_scope());
  VLOG_IF(1, result != kErrorOk) << __func__ << " failed: " << result;
  return result == kErrorOk;
}

}  // namespace updater
