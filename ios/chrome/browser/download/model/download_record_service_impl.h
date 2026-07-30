// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

namespace base {
class FilePath;
}  // namespace base

class DownloadRecordStore;

// Implementation class that manages download records with persistent storage.
class DownloadRecordServiceImpl : public DownloadRecordService,
                                  public web::DownloadTaskObserver {
 public:
  explicit DownloadRecordServiceImpl(const base::FilePath& profile_path);

  DownloadRecordServiceImpl(const DownloadRecordServiceImpl&) = delete;
  DownloadRecordServiceImpl& operator=(const DownloadRecordServiceImpl&) =
      delete;

  ~DownloadRecordServiceImpl() override;

  // DownloadRecordService implementation.
  void RecordDownload(web::DownloadTask* task) override;
  void GetAllDownloadsAsync(DownloadRecordsCallback callback) override;
  void GetDownloadByIdAsync(const std::string& download_id,
                            DownloadRecordCallback callback) override;
  void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback()) override;
  void GetDownloadsPageAsync(const DownloadRecordQuery& query,
                             DownloadRecordsPageCallback callback) override;
  void GetDownloadsCountAsync(std::optional<DownloadFilterType> filter,
                              DownloadRecordsCountCallback callback) override;
  void UpdateDownloadFilePathAsync(
      const std::string& download_id,
      const base::FilePath& file_path,
      CompletionCallback callback = CompletionCallback()) override;
  web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const override;
  void AddObserver(DownloadRecordObserver* observer) override;
  void RemoveObserver(DownloadRecordObserver* observer) override;

  // web::DownloadTaskObserver implementation.
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

 private:
  // Notifies observers.
  void NotifyDownloadAdded(const DownloadRecord& record);
  void NotifyDownloadUpdated(const DownloadRecord& record);
  void NotifyDownloadsRemoved(
      const std::vector<std::string_view>& download_ids);

  // Reply for the asynchronous InsertRecord write started by
  // `RecordDownload()`, run on the main sequence. Observes `task` and
  // notifies observers when the insert succeeded.
  void OnRecordInserted(base::WeakPtr<web::DownloadTask> weak_task,
                        const DownloadRecord& record,
                        bool success);

  // Snapshot of `IsDownloadListPaginationEnabled()` taken at construction
  // time, so an in-process Finch flip cannot change the path chosen at
  // startup. Every pagination-gated code path reads this member, not the
  // live function.
  const bool pagination_enabled_;

  // Task runner for database operations. `store_` is bound to it at
  // construction.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  // Owns the SQLite-backed database and the in-memory record cache. All
  // CRUD runs on `database_task_runner_` via `AsyncCall`; `SequenceBound`
  // also destroys the store on that sequence, so any in-flight DB-task
  // observes a live store even after `this` is gone on the main thread.
  // The two preceding members are declared first because they feed this
  // constructor (member init runs in declaration order).
  base::SequenceBound<DownloadRecordStore> store_;

  // ObserverList for download record changes.
  base::ObserverList<DownloadRecordObserver, /* check_empty= */ true>
      observers_;
  // Observation for download tasks.
  base::ScopedMultiSourceObservation<web::DownloadTask,
                                     web::DownloadTaskObserver>
      download_task_observations_{this};

  // Main thread sequence checker for public API calls.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<DownloadRecordServiceImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_
