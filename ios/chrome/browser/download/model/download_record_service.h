// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/download/model/download_record_query.h"

namespace base {
class FilePath;
}  // namespace base

namespace web {
class DownloadTask;
}  // namespace web

struct DownloadRecord;
class DownloadRecordObserver;

// Base class for download record service that defines the public interface.
class DownloadRecordService : public KeyedService {
 public:
  // Callback types for async operations.
  using DownloadRecordsCallback =
      base::OnceCallback<void(std::vector<DownloadRecord>)>;
  using DownloadRecordCallback =
      base::OnceCallback<void(std::optional<DownloadRecord>)>;
  using CompletionCallback = base::OnceCallback<void(bool success)>;

  // Page-result callback for pagination reads.
  using DownloadRecordsPageCallback =
      base::OnceCallback<void(std::vector<DownloadRecord>)>;
  // Count callback for pagination reads.
  using DownloadRecordsCountCallback = base::OnceCallback<void(size_t)>;

  DownloadRecordService() = default;

  DownloadRecordService(const DownloadRecordService&) = delete;
  DownloadRecordService& operator=(const DownloadRecordService&) = delete;

  ~DownloadRecordService() override = default;

  // Records a new download and start observing it.
  virtual void RecordDownload(web::DownloadTask* task) = 0;
  // Retrieves all downloads. Callback is invoked on the calling thread.
  virtual void GetAllDownloadsAsync(DownloadRecordsCallback callback) = 0;
  // Retrieves a download by ID. Callback is invoked on the calling thread.
  virtual void GetDownloadByIdAsync(const std::string& download_id,
                                    DownloadRecordCallback callback) = 0;
  // Removes a download by ID. Callback is invoked on the calling thread.
  virtual void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Returns one keyset-paginated page of download records.
  //
  // Contract:
  //   * Ordering: rows are returned in DESC (created_time, download_id)
  //     order; ordering is stable across pages even when concurrent
  //     inserts/updates happen on intervening rows.
  //   * Cursor: pass an empty `query` to fetch the first page; for
  //     subsequent pages, populate `cursor_created_time` and
  //     `cursor_download_id` with the values of the last returned row.
  //     The cursor refers to ordering keys only and is not tied to a row
  //     identity, so deleting the cursor row does not invalidate
  //     continuation.
  //   * Filter: when `filter_type` is set, only rows matching the filter
  //     are returned. When `name_query` is set, a case-insensitive
  //     substring match on the normalized file name is applied.
  //   * Freshness: rows whose download is currently held in the in-memory
  //     active cache (e.g. an in-progress download whose byte-progress
  //     update has not yet been flushed to disk) are returned with the
  //     cached value taking precedence over the persisted row. Incognito
  //     (off-the-record) records are never included because they are not
  //     persisted.
  //   * Threading: `callback` is invoked on the calling sequence and the
  //     method never blocks the calling thread.
  //   * Errors: if the database has not yet finished asynchronous
  //     initialization, `callback` is invoked with an empty vector.
  //   * Feature gating: this method will be gated by
  //     `kDownloadListPagination` once the paginated reader lands. In
  //     this CL the implementation is a placeholder that always posts
  //     an empty vector to `callback` on the calling sequence
  //     regardless of flag state; callers should continue to use
  //     `GetAllDownloadsAsync` until the follow-up CL ships the real
  //     reader.
  virtual void GetDownloadsPageAsync(const DownloadRecordQuery& query,
                                     DownloadRecordsPageCallback callback) = 0;

  // Returns the total count of persisted records matching `filter`.
  //
  // Contract:
  //   * Counts persisted records only; incognito (in-memory only)
  //     records are excluded.
  //   * When `filter` is unset or `kAll`, counts all persisted records.
  //   * Threading: `callback` is invoked on the calling sequence and the
  //     method never blocks the calling thread.
  //   * Errors: if the database has not yet finished asynchronous
  //     initialization, `callback` is invoked with 0.
  //   * Feature gating: this method will be gated by
  //     `kDownloadListPagination` once the paginated reader lands. In
  //     this CL the implementation is a placeholder that always posts
  //     0 to `callback` on the calling sequence regardless of flag
  //     state.
  virtual void GetDownloadsCountAsync(
      std::optional<DownloadFilterType> filter,
      DownloadRecordsCountCallback callback) = 0;

  // Updates the file path for a download record by ID.
  // Callback is invoked on the calling thread.
  virtual void UpdateDownloadFilePathAsync(
      const std::string& download_id,
      const base::FilePath& file_path,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Gets download task by ID.
  virtual web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const = 0;

  // Observer management.
  virtual void AddObserver(DownloadRecordObserver* observer) = 0;
  virtual void RemoveObserver(DownloadRecordObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
