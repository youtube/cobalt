// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

scoped_refptr<base::UpdateableSequencedTaskRunner> CreateStorageTaskRunner() {
  // This uses BLOCK_SHUTDOWN as some data deletion operations may be running
  // when the browser is closed, and we want to ensure all data is deleted
  // correctly. Additionally, we use MUST_USE_FOREGROUND to avoid priority
  // inversions if a task is already running when the priority is increased.
  return base::ThreadPool::CreateUpdateableSequencedTaskRunner(
      base::TaskTraits(base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                       base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                       base::ThreadPolicy::MUST_USE_FOREGROUND));
}

}  // namespace

AggregationServiceImpl::AggregationServiceImpl(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    StoragePartitionImpl* storage_partition)
    : storage_task_runner_(CreateStorageTaskRunner()),
      storage_(
          // Ensure storage is constructed first (and destroyed last) so we can
          // safely pass `this` as an `AggregationServiceStorageContext` in the
          // below constructors.
          // TODO(alexmt): Pass the storage directly to avoid an extra wrapper.
          base::SequenceBound<AggregationServiceStorageSql>(
              storage_task_runner_,
              run_in_memory,
              user_data_directory,
              base::DefaultClock::GetInstance())),
      scheduler_(std::make_unique<AggregatableReportScheduler>(
          /*storage_context=*/this,
          // `base::Unretained` is safe as the scheduler is owned by `this`.
          base::BindRepeating(
              &AggregationServiceImpl::OnScheduledReportTimeReached,
              base::Unretained(this)))),
      assembler_(std::make_unique<AggregatableReportAssembler>(
          /*storage_context=*/this,
          storage_partition)),
      sender_(std::make_unique<AggregatableReportSender>(storage_partition)) {}

AggregationServiceImpl::~AggregationServiceImpl() = default;

// static
std::unique_ptr<AggregationServiceImpl>
AggregationServiceImpl::CreateForTesting(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportScheduler> scheduler,
    std::unique_ptr<AggregatableReportAssembler> assembler,
    std::unique_ptr<AggregatableReportSender> sender) {
  return base::WrapUnique<AggregationServiceImpl>(new AggregationServiceImpl(
      run_in_memory, user_data_directory, clock, std::move(scheduler),
      std::move(assembler), std::move(sender)));
}

AggregationServiceImpl::AggregationServiceImpl(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportScheduler> scheduler,
    std::unique_ptr<AggregatableReportAssembler> assembler,
    std::unique_ptr<AggregatableReportSender> sender)
    : storage_task_runner_(CreateStorageTaskRunner()),
      storage_(base::SequenceBound<AggregationServiceStorageSql>(
          storage_task_runner_,
          run_in_memory,
          user_data_directory,
          clock)),
      scheduler_(std::move(scheduler)),
      assembler_(std::move(assembler)),
      sender_(std::move(sender)) {}

void AggregationServiceImpl::AssembleReport(
    AggregatableReportRequest report_request,
    AssemblyCallback callback) {
  assembler_->AssembleReport(std::move(report_request), std::move(callback));
}

void AggregationServiceImpl::SendReport(const GURL& url,
                                        const AggregatableReport& report,
                                        SendCallback callback) {
  SendReport(url, base::Value(report.GetAsJson()), std::move(callback));
}

void AggregationServiceImpl::SendReport(const GURL& url,
                                        const base::Value& contents,
                                        SendCallback callback) {
  sender_->SendReport(url, contents, std::move(callback));
}

const base::SequenceBound<AggregationServiceStorage>&
AggregationServiceImpl::GetStorage() {
  return storage_;
}

void AggregationServiceImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  // When a clear data task is queued or running, we use a higher priority.
  ++num_pending_clear_data_tasks_;
  storage_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  storage_.AsyncCall(&AggregationServiceStorage::ClearDataBetween)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(std::move(done).Then(
          base::BindOnce(&AggregationServiceImpl::OnClearDataComplete,
                         weak_factory_.GetWeakPtr())));
}

void AggregationServiceImpl::OnClearDataComplete() {
  DCHECK_GT(num_pending_clear_data_tasks_, 0);
  --num_pending_clear_data_tasks_;

  // No more clear data tasks, so we can reset the priority.
  if (num_pending_clear_data_tasks_ == 0)
    storage_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  NotifyRequestStorageModified();
}

void AggregationServiceImpl::ScheduleReport(
    AggregatableReportRequest report_request) {
  scheduler_->ScheduleRequest(std::move(report_request));
  NotifyRequestStorageModified();
}

void AggregationServiceImpl::AssembleAndSendReport(
    AggregatableReportRequest report_request) {
  AssembleAndSendReportImpl(std::move(report_request), /*id=*/absl::nullopt,
                            /*done=*/base::DoNothing());
}

void AggregationServiceImpl::AssembleAndSendReportImpl(
    AggregatableReportRequest report_request,
    absl::optional<AggregationServiceStorage::RequestId> request_id,
    base::OnceClosure done) {
  GURL reporting_url = report_request.GetReportingUrl();
  AssembleReport(
      std::move(report_request),
      base::BindOnce(
          &AggregationServiceImpl::OnReportAssemblyComplete,
          // `base::Unretained` is safe as the assembler is owned by `this`.
          base::Unretained(this), std::move(done), request_id,
          std::move(reporting_url)));
}

void AggregationServiceImpl::OnScheduledReportTimeReached(
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  AssembleAndSendReports(std::move(requests_and_ids),
                         /*done=*/base::DoNothing());
}

void AggregationServiceImpl::OnReportAssemblyComplete(
    base::OnceClosure done,
    absl::optional<AggregationServiceStorage::RequestId> request_id,
    GURL reporting_url,
    AggregatableReportRequest report_request,
    absl::optional<AggregatableReport> report,
    AggregatableReportAssembler::AssemblyStatus status) {
  DCHECK_EQ(report.has_value(),
            status == AggregatableReportAssembler::AssemblyStatus::kOk);
  if (!report.has_value()) {
    std::move(done).Run();

    bool will_retry =
        request_id.has_value() &&
        scheduler_->NotifyInProgressRequestFailed(
            request_id.value(), report_request.failed_send_attempts());
    if (!will_retry) {
      NotifyReportHandled(
          std::move(report_request), request_id,
          /*report=*/absl::nullopt,
          AggregationServiceObserver::ReportStatus::kFailedToAssemble);
    }
    if (request_id.has_value()) {
      NotifyRequestStorageModified();
    }
    return;
  }

  // TODO(crbug.com/1354220): Consider checking with the browser client if
  // reporting is allowed before sending. We don't currently have the top-frame
  // origin to perform this check.
  base::Value value(report->GetAsJson());
  SendReport(reporting_url, value,
             /*callback=*/
             base::BindOnce(
                 &AggregationServiceImpl::OnReportSendingComplete,
                 // `base::Unretained` is safe as the sender is owned by `this`.
                 base::Unretained(this), std::move(done),
                 std::move(report_request), request_id, std::move(*report)));
}

void AggregationServiceImpl::OnReportSendingComplete(
    base::OnceClosure done,
    AggregatableReportRequest report_request,
    absl::optional<AggregationServiceStorage::RequestId> request_id,
    AggregatableReport report,
    AggregatableReportSender::RequestStatus status) {
  std::move(done).Run();

  AggregationServiceObserver::ReportStatus observer_status;
  bool will_retry;
  switch (status) {
    case AggregatableReportSender::RequestStatus::kOk:
      observer_status = AggregationServiceObserver::ReportStatus::kSent;
      if (request_id.has_value()) {
        scheduler_->NotifyInProgressRequestSucceeded(request_id.value());
      }
      will_retry = false;
      break;
    case AggregatableReportSender::RequestStatus::kNetworkError:
    case AggregatableReportSender::RequestStatus::kServerError:
      observer_status = AggregationServiceObserver::ReportStatus::kFailedToSend;
      will_retry =
          request_id.has_value() &&
          scheduler_->NotifyInProgressRequestFailed(
              request_id.value(), report_request.failed_send_attempts());
      break;
  }
  if (!will_retry) {
    NotifyReportHandled(std::move(report_request), request_id,
                        std::move(report), observer_status);
  }
  if (request_id.has_value()) {
    NotifyRequestStorageModified();
  }
}

void AggregationServiceImpl::SetPublicKeysForTesting(
    const GURL& url,
    const PublicKeyset& keyset) {
  storage_.AsyncCall(&AggregationServiceStorage::SetPublicKeys)
      .WithArgs(url, keyset);
}

void AggregationServiceImpl::AssembleAndSendReports(
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids,
    base::RepeatingClosure done) {
  for (AggregationServiceStorage::RequestAndId& elem : requests_and_ids) {
    AssembleAndSendReportImpl(std::move(elem.request), elem.id, done);
  }
}

void AggregationServiceImpl::GetPendingReportRequestsForWebUI(
    base::OnceCallback<
        void(std::vector<AggregationServiceStorage::RequestAndId>)> callback) {
  // Enforce the limit on the number of reports shown in the WebUI to prevent
  // the page from consuming too much memory.
  storage_.AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
      .WithArgs(/*not_after_time=*/base::Time::Max(), /*limit=*/1000)
      .Then(std::move(callback));
}

void AggregationServiceImpl::SendReportsForWebUI(
    const std::vector<AggregationServiceStorage::RequestId>& ids,
    base::OnceClosure reports_sent_callback) {
  storage_.AsyncCall(&AggregationServiceStorage::GetRequests)
      .WithArgs(ids)
      .Then(base::BindOnce(
          &AggregationServiceImpl::OnGetRequestsToSendFromWebUI,
          weak_factory_.GetWeakPtr(), std::move(reports_sent_callback)));
}

void AggregationServiceImpl::OnGetRequestsToSendFromWebUI(
    base::OnceClosure reports_sent_callback,
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  if (requests_and_ids.empty()) {
    std::move(reports_sent_callback).Run();
    return;
  }

  auto barrier = base::BarrierClosure(requests_and_ids.size(),
                                      std::move(reports_sent_callback));
  AssembleAndSendReports(std::move(requests_and_ids), std::move(barrier));
}

void AggregationServiceImpl::AddObserver(AggregationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AggregationServiceImpl::RemoveObserver(
    AggregationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AggregationServiceImpl::NotifyReportHandled(
    const AggregatableReportRequest& request,
    absl::optional<AggregationServiceStorage::RequestId> request_id,
    const absl::optional<AggregatableReport>& report,
    AggregationServiceObserver::ReportStatus status) {
  bool is_scheduled_request = request_id.has_value();
  bool did_request_succeed =
      status == AggregationServiceObserver::ReportStatus::kSent;

  if (is_scheduled_request) {
    base::UmaHistogramEnumeration(
        "PrivacySandbox.AggregationService.ScheduledRequests.Status", status);
  } else {
    base::UmaHistogramEnumeration(
        "PrivacySandbox.AggregationService.UnscheduledRequests.Status", status);
  }

  if (is_scheduled_request && did_request_succeed) {
    base::UmaHistogramExactLinear(
        "PrivacySandbox.AggregationService.ScheduledRequests."
        "NumRetriesBeforeSuccess",
        request.failed_send_attempts(),
        /*exclusive_max=*/AggregatableReportScheduler::kMaxRetries + 1);
  }

  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnReportHandled(request, request_id, report,
                             /*report_handled_time=*/now, status);
  }
}

void AggregationServiceImpl::NotifyRequestStorageModified() {
  for (auto& observer : observers_) {
    observer.OnRequestStorageModified();
  }
}

}  // namespace content
