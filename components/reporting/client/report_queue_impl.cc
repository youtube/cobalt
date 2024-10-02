// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_impl.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {
namespace {

// UTC time of 2122-01-01T00:00:00Z since Unix epoch 1970-01-01T00:00:00Z in
// microseconds.
constexpr int64_t kTime2122 = 4'796'668'800'000'000;

// Calls |record_producer|, checks the result and in case of success, forwards
// it to the storage. In production code should be invoked asynchronously, on a
// thread pool (no synchronization expected).
void AddRecordToStorage(scoped_refptr<StorageModuleInterface> storage,
                        Priority priority,
                        std::string dm_token,
                        Destination destination,
                        int64_t reserved_space,
                        ReportQueue::RecordProducer record_producer,
                        StorageModuleInterface::EnqueueCallback callback) {
  // Generate record data.
  auto record_result = std::move(record_producer).Run();
  if (!record_result.ok()) {
    std::move(callback).Run(record_result.status());
    return;
  }

  // Augment data.
  Record record;
  *record.mutable_data() = std::move(record_result.ValueOrDie());
  record.set_destination(destination);
  if (reserved_space > 0L) {
    record.set_reserved_space(reserved_space);
  }

  // Additional record augmentation for keeping local record copy.
  // Note: that must be done before calling `storage->AddRecord` below,
  // because later the handler might call it with no need to set this flag.
  switch (destination) {
    case LOG_UPLOAD:
      // It would be better to base the decision on `upload_settings` presence
      // in the event, but that would require protobuf reflecion, that is not
      // included in Chromium build. So instead we just use `destination`.
      record.set_needs_local_unencrypted_copy(true);
      break;
    default:  // Do nothing.
      break;
  }

  // |record| with no DM token is assumed to be associated with device DM token
  if (!dm_token.empty()) {
    *record.mutable_dm_token() = std::move(dm_token);
  }

  // Calculate timestamp in microseconds - to match Spanner expectations.
  const int64_t time_since_epoch_us =
      base::Time::Now().ToJavaTime() * base::Time::kMicrosecondsPerMillisecond;
  if (time_since_epoch_us > kTime2122) {
    // Unusual timestamp. Reject the record even though the record is good
    // otherwise, because we can't obtain a reasonable timestamp. We have this
    // code block here because server very occasionally detects very large
    // timestamps. The reason could come from occasional irregular system time.
    // Filtering out irregular timestamps here should address the problem
    // without leaving timestamp-related bugs in the ERP undiscovered (should
    // there be any).
    base::UmaHistogramBoolean("Browser.ERP.UnusualEnqueueTimestamp", true);
    std::move(callback).Run(Status(
        error::FAILED_PRECONDITION,
        base::StrCat(
            {"Abnormal system timestamp obtained. Microseconds since epoch: ",
             base::NumberToString(time_since_epoch_us)})));
    return;
  }
  record.set_timestamp_us(time_since_epoch_us);
  if (!record_result.ok()) {
    std::move(callback).Run(record_result.status());
    return;
  }

  // Add resulting Record to the storage.
  storage->AddRecord(priority, std::move(record), std::move(callback));
}
}  // namespace

void ReportQueueImpl::Create(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage,
    base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)> cb) {
  std::move(cb).Run(base::WrapUnique<ReportQueueImpl>(
      new ReportQueueImpl(std::move(config), storage)));
}

ReportQueueImpl::ReportQueueImpl(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage)
    : config_(std::move(config)), storage_(storage) {}

ReportQueueImpl::~ReportQueueImpl() = default;

void ReportQueueImpl::AddProducedRecord(RecordProducer record_producer,
                                        Priority priority,
                                        EnqueueCallback callback) const {
  const Status status = config_->CheckPolicy();
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  if (priority == Priority::UNDEFINED_PRIORITY) {
    std::move(callback).Run(
        Status(error::INVALID_ARGUMENT, "Priority must be defined"));
    return;
  }

  // Execute |record_producer| on arbitrary thread, analyze the result and send
  // it to the Storage, returning with the callback.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&AddRecordToStorage, storage_, priority,
                     config_->dm_token(), config_->destination(),
                     config_->reserved_space(), std::move(record_producer),
                     std::move(callback)));
}

void ReportQueueImpl::Flush(Priority priority, FlushCallback callback) {
  storage_->Flush(priority, std::move(callback));
}

base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
ReportQueueImpl::PrepareToAttachActualQueue() const {
  NOTREACHED();
  return base::BindOnce(
      [](StatusOr<std::unique_ptr<ReportQueue>>) { NOTREACHED(); });
}

// Implementation of SpeculativeReportQueueImpl::PendingRecordProducer

SpeculativeReportQueueImpl::PendingRecordProducer::PendingRecordProducer(
    RecordProducer producer,
    EnqueueCallback callback,
    Priority priority)
    : record_producer(std::move(producer)),
      record_callback(std::move(callback)),
      record_priority(priority) {}

SpeculativeReportQueueImpl::PendingRecordProducer::PendingRecordProducer(
    PendingRecordProducer&& other)
    : record_producer(std::move(other.record_producer)),
      record_callback(std::move(other.record_callback)),
      record_priority(other.record_priority) {}

SpeculativeReportQueueImpl::PendingRecordProducer::~PendingRecordProducer() =
    default;

SpeculativeReportQueueImpl::PendingRecordProducer&
SpeculativeReportQueueImpl::PendingRecordProducer::operator=(
    PendingRecordProducer&& other) {
  record_producer = std::move(other.record_producer);
  record_callback = std::move(other.record_callback);
  record_priority = other.record_priority;
  return *this;
}

// static
std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>
SpeculativeReportQueueImpl::Create() {
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
  return std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>(
      new SpeculativeReportQueueImpl(sequenced_task_runner),
      base::OnTaskRunnerDeleter(sequenced_task_runner));
}

SpeculativeReportQueueImpl::SpeculativeReportQueueImpl(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : sequenced_task_runner_(sequenced_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SpeculativeReportQueueImpl::~SpeculativeReportQueueImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PurgePendingProducers(
      Status(error::DATA_LOSS, "The queue is being destructed"));
}

void SpeculativeReportQueueImpl::Flush(Priority priority,
                                       FlushCallback callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Priority priority, FlushCallback callback,
             base::WeakPtr<SpeculativeReportQueueImpl> self) {
            if (!self) {
              std::move(callback).Run(
                  Status(error::UNAVAILABLE, "Queue has been destructed"));
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            if (!self->actual_report_queue_.has_value()) {
              std::move(callback).Run(Status(error::FAILED_PRECONDITION,
                                             "ReportQueue is not ready yet."));
              return;
            }
            const std::unique_ptr<ReportQueue>& report_queue =
                self->actual_report_queue_.value();
            report_queue->Flush(priority, std::move(callback));
          },
          priority, std::move(callback), weak_ptr_factory_.GetWeakPtr()));
}

void SpeculativeReportQueueImpl::AddProducedRecord(
    RecordProducer record_producer,
    Priority priority,
    EnqueueCallback callback) const {
  // Invoke producer on a thread pool, then enqueue record on sequenced task
  // runner.
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeculativeReportQueueImpl::MaybeEnqueueRecordProducer,
                     weak_ptr_factory_.GetWeakPtr(), priority,
                     std::move(callback), std::move(record_producer)));
}

void SpeculativeReportQueueImpl::MaybeEnqueueRecordProducer(
    Priority priority,
    EnqueueCallback callback,
    RecordProducer record_producer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!actual_report_queue_.has_value()) {
    // Queue is not ready yet, store the record in the memory queue.
    pending_record_producers_.emplace(std::move(record_producer),
                                      std::move(callback), priority);
    return;
  }
  // Queue is ready. If memory queue is empty, just forward the record.
  if (pending_record_producers_.empty()) {
    const std::unique_ptr<ReportQueue>& report_queue =
        actual_report_queue_.value();
    report_queue->AddProducedRecord(std::move(record_producer), priority,
                                    std::move(callback));
    return;
  }
  // If memory queue is not empty, attach the new record at the
  // end and initiate enqueuing of everything from there.
  pending_record_producers_.emplace(std::move(record_producer),
                                    std::move(callback), priority);
  EnqueuePendingRecordProducers();
}

void SpeculativeReportQueueImpl::EnqueuePendingRecordProducers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(actual_report_queue_.has_value());
  if (pending_record_producers_.empty()) {
    return;
  }
  const std::unique_ptr<ReportQueue>& report_queue =
      actual_report_queue_.value();
  auto head = std::move(pending_record_producers_.front());
  pending_record_producers_.pop();
  if (pending_record_producers_.empty()) {
    // Last of the pending records.
    report_queue->AddProducedRecord(std::move(head.record_producer),
                                    head.record_priority,
                                    std::move(head.record_callback));
    return;
  }
  report_queue->AddProducedRecord(
      std::move(head.record_producer), head.record_priority,
      base::BindPostTask(
          sequenced_task_runner_,
          base::BindOnce(
              [](base::WeakPtr<const SpeculativeReportQueueImpl> self,
                 EnqueueCallback callback, Status status) {
                if (!status.ok()) {
                  std::move(callback).Run(status);
                  return;
                }
                if (!self) {
                  std::move(callback).Run(
                      Status(error::UNAVAILABLE, "Queue has been destructed"));
                  return;
                }
                std::move(callback).Run(status);
                self->EnqueuePendingRecordProducers();
              },
              weak_ptr_factory_.GetWeakPtr(),
              std::move(head.record_callback))));
}

base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
SpeculativeReportQueueImpl::PrepareToAttachActualQueue() const {
  return base::BindPostTask(
      sequenced_task_runner_,
      base::BindOnce(&SpeculativeReportQueueImpl::AttachActualQueue,
                     weak_ptr_factory_.GetMutableWeakPtr()));
}

void SpeculativeReportQueueImpl::AttachActualQueue(
    StatusOr<std::unique_ptr<ReportQueue>> status_or_actual_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (actual_report_queue_.has_value()) {
    // Already attached, do nothing.
    return;
  }
  if (!status_or_actual_queue.ok()) {
    // Failed to create actual queue.
    // Flush all pending records with this status.
    PurgePendingProducers(status_or_actual_queue.status());
    return;
  }
  // Actual report queue succeeded, store it (never to change later).
  actual_report_queue_ = std::move(status_or_actual_queue.ValueOrDie());
  EnqueuePendingRecordProducers();
}

void SpeculativeReportQueueImpl::PurgePendingProducers(Status status) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!pending_record_producers_.empty()) {
    auto head = std::move(pending_record_producers_.front());
    pending_record_producers_.pop();
    std::move(head.record_callback).Run(status);
  }
}
}  // namespace reporting
