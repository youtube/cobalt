// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace storage {

// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxFileSystemBackend
    : public FileSystemBackend {
 public:
  explicit SandboxFileSystemBackend(SandboxFileSystemBackendDelegate* delegate);

  SandboxFileSystemBackend(const SandboxFileSystemBackend&) = delete;
  SandboxFileSystemBackend& operator=(const SandboxFileSystemBackend&) = delete;

  ~SandboxFileSystemBackend() override;

  // FileSystemBackend overrides.
  bool CanHandleType(FileSystemType type) const override;
  void Initialize(FileSystemContext* context) override;
  void ResolveURL(const FileSystemURL& url,
                  OpenFileSystemMode mode,
                  ResolveURLCallback callback) override;
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) override;
  WatcherManager* GetWatcherManager(FileSystemType type) override;
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) override;
  std::unique_ptr<FileSystemOperation> CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(FileSystemType type) const override;
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) const override;
  std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset,
      FileSystemContext* context) const override;
  FileSystemQuotaUtil* GetQuotaUtil() override;
  const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const override;
  const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const override;
  const AccessObserverList* GetAccessObservers(
      FileSystemType type) const override;

  // Returns a StorageKey enumerator of this backend.
  // This method can only be called on the file thread.
  SandboxFileSystemBackendDelegate::StorageKeyEnumerator*
  CreateStorageKeyEnumerator();

 private:
  raw_ptr<SandboxFileSystemBackendDelegate> delegate_;  // Not owned.
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_H_
