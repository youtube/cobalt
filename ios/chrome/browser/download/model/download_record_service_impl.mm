// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_impl.h"

#import <optional>
#import <string_view>
#import <utility>
#import <vector>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record_store.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#pragma mark - Public

DownloadRecordServiceImpl::DownloadRecordServiceImpl(
    const base::FilePath& profile_path)
    : pagination_enabled_(IsDownloadListPaginationEnabled()),
      database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      store_(database_task_runner_, pagination_enabled_) {
  CHECK(IsDownloadListEnabled());
  CHECK(!profile_path.empty());

  // Initialize the database, then run the appropriate startup cleanup
  // path. Both calls are dispatched on `database_task_runner_` via
  // `AsyncCall`, which guarantees ordering.
  store_.AsyncCall(&DownloadRecordStore::InitializeDatabase)
      .WithArgs(profile_path);
  if (pagination_enabled_) {
    store_.AsyncCall(&DownloadRecordStore::MarkUnfinishedDownloadsAsFailed);
  } else {
    store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords);
  }
}

DownloadRecordServiceImpl::~DownloadRecordServiceImpl() = default;

void DownloadRecordServiceImpl::RecordDownload(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord record = DownloadRecord(task);
  // Sets creation time when we first record the download.
  record.created_time = base::Time::Now();

  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(base::BindOnce(&DownloadRecordServiceImpl::OnRecordInserted,
                           weak_ptr_factory_.GetWeakPtr(), task->GetWeakPtr(),
                           record));
}

void DownloadRecordServiceImpl::OnRecordInserted(
    base::WeakPtr<web::DownloadTask> weak_task,
    const DownloadRecord& record,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // `weak_task` is held as a WeakPtr because the DownloadTask may be destroyed
  // during the asynchronous InsertRecord write (e.g. the download is
  // cancelled, which posts CleanupCurrentDownload -> task_.reset()). The
  // service is not yet observing the task at this point, so it receives no
  // OnDownloadDestroyed callback. If the task is gone, the pointer is null and
  // we skip safely instead of dereferencing freed memory.
  web::DownloadTask* task = weak_task.get();
  if (!success || !task) {
    return;
  }

  // Guard against double-registration: the same task may be retried (e.g.
  // user taps "Try Again"), which would call RecordDownload again with the
  // same DownloadTask pointer. AddObservation CHECKs that the source is not
  // already observed, so skip it if we are already tracking this task.
  if (download_task_observations_.IsObservingSource(task)) {
    return;
  }

  download_task_observations_.AddObservation(task);
  NotifyDownloadAdded(record);
}

void DownloadRecordServiceImpl::GetAllDownloadsAsync(
    DownloadRecordsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  store_.AsyncCall(&DownloadRecordStore::GetAllFromCache)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::GetDownloadByIdAsync(
    const std::string& download_id,
    DownloadRecordCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs(download_id)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::RemoveDownloadByIdAsync(
    const std::string& download_id,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Find and remove from observed tasks if present.
  web::DownloadTask* task_to_remove = GetDownloadTaskById(download_id);
  if (task_to_remove) {
    download_task_observations_.RemoveObservation(task_to_remove);
  }

  store_.AsyncCall(&DownloadRecordStore::DeleteRecord)
      .WithArgs(download_id)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             const std::string& download_id, CompletionCallback callback,
             bool success) {
            if (service && success) {
              service->NotifyDownloadsRemoved({std::string_view(download_id)});
            }
            if (callback) {
              std::move(callback).Run(success);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), download_id, std::move(callback)));
}

void DownloadRecordServiceImpl::GetDownloadsPageAsync(
    const DownloadRecordQuery& query,
    DownloadRecordsPageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Paginated readers require kDownloadListPagination.
  DCHECK(pagination_enabled_);
  store_.AsyncCall(&DownloadRecordStore::GetDownloadsPage)
      .WithArgs(query)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::GetDownloadsCountAsync(
    std::optional<DownloadFilterType> filter,
    DownloadRecordsCountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(pagination_enabled_);

  // Translate the optional filter into a count-only query. The cursor
  // fields are ignored by the DB layer for count queries.
  DownloadRecordQuery query;
  query.filter_type = filter;

  store_.AsyncCall(&DownloadRecordStore::GetDownloadsCount)
      .WithArgs(query)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::UpdateDownloadFilePathAsync(
    const std::string& download_id,
    const base::FilePath& file_path,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  store_.AsyncCall(&DownloadRecordStore::UpdateFilePathInRecord)
      .WithArgs(download_id, file_path)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             CompletionCallback callback,
             std::optional<DownloadRecord> updated_record) {
            bool success = updated_record.has_value();
            if (service && success) {
              service->NotifyDownloadUpdated(updated_record.value());
            }
            if (callback) {
              std::move(callback).Run(success);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

web::DownloadTask* DownloadRecordServiceImpl::GetDownloadTaskById(
    std::string_view download_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Search through all observed download tasks.
  for (web::DownloadTask* task : download_task_observations_.sources()) {
    if (base::SysNSStringToUTF8(task->GetIdentifier()) == download_id) {
      return task;
    }
  }
  return nullptr;
}

#pragma mark - Observer Management

void DownloadRecordServiceImpl::AddObserver(DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.AddObserver(observer);
}

void DownloadRecordServiceImpl::RemoveObserver(
    DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.RemoveObserver(observer);
}

#pragma mark - web::DownloadTaskObserver

void DownloadRecordServiceImpl::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord updated_record = DownloadRecord(task);

  store_.AsyncCall(&DownloadRecordStore::UpdateRecord)
      .WithArgs(updated_record)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             std::optional<DownloadRecord> record_opt) {
            if (service && record_opt.has_value()) {
              service->NotifyDownloadUpdated(record_opt.value());
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void DownloadRecordServiceImpl::OnDownloadDestroyed(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);
  if (pagination_enabled_) {
    // Extract the id BEFORE `RemoveObservation`, which may drop the
    // last handle on `task`. `EvictOnDestroy` is FIFO-ordered with every
    // other CRUD task on the database sequence (see
    // download_record_store.h), so a still-pending `UpdateRecord` for
    // the same id finishes first and cannot resurrect the entry.
    std::string download_id = base::SysNSStringToUTF8(task->GetIdentifier());
    store_.AsyncCall(&DownloadRecordStore::EvictOnDestroy)
        .WithArgs(std::move(download_id));
  }
  download_task_observations_.RemoveObservation(task);
}

#pragma mark - Private

void DownloadRecordServiceImpl::NotifyDownloadAdded(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadAdded, record);
}

void DownloadRecordServiceImpl::NotifyDownloadUpdated(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadUpdated, record);
}

void DownloadRecordServiceImpl::NotifyDownloadsRemoved(
    const std::vector<std::string_view>& download_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadsRemoved, download_ids);
}
