// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_PROVIDER_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_PROVIDER_ASYNC_FILE_UTIL_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash {
namespace file_system_provider {

// TODO(mtomasz): Remove this namespace.
namespace internal {

// The implementation of storage::AsyncFileUtil for provided file systems. It is
// created one per Chrome process. It is responsible for routing calls to the
// correct profile, and then to the correct profided file system.
//
// This class should be called AsyncFileUtil, without the Provided prefix. This
// is impossible, though because of GYP limitations. There must not be two files
// with the same name in a Chromium tree.
// See: https://code.google.com/p/gyp/issues/detail?id=384
//
// All of the methods should be called on the IO thread.
class ProviderAsyncFileUtil : public storage::AsyncFileUtil {
 public:
  ProviderAsyncFileUtil();

  ProviderAsyncFileUtil(const ProviderAsyncFileUtil&) = delete;
  ProviderAsyncFileUtil& operator=(const ProviderAsyncFileUtil&) = delete;

  ~ProviderAsyncFileUtil() override;

  // storage::AsyncFileUtil overrides.
  void CreateOrOpen(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      uint32_t file_flags,
      CreateOrOpenCallback callback) override;
  void EnsureFileExists(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      EnsureFileExistsCallback callback) override;
  void CreateDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      StatusCallback callback) override;
  void GetFileInfo(std::unique_ptr<storage::FileSystemOperationContext> context,
                   const storage::FileSystemURL& url,
                   int fields,
                   GetFileInfoCallback callback) override;
  void ReadDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      ReadDirectoryCallback callback) override;
  void Touch(std::unique_ptr<storage::FileSystemOperationContext> context,
             const storage::FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             StatusCallback callback) override;
  void Truncate(std::unique_ptr<storage::FileSystemOperationContext> context,
                const storage::FileSystemURL& url,
                int64_t length,
                StatusCallback callback) override;
  void CopyFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      CopyFileProgressCallback progress_callback,
      StatusCallback callback) override;
  void MoveFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      StatusCallback callback) override;
  void CopyInForeignFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      StatusCallback callback) override;
  void DeleteFile(std::unique_ptr<storage::FileSystemOperationContext> context,
                  const storage::FileSystemURL& url,
                  StatusCallback callback) override;
  void DeleteDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;
  void DeleteRecursively(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;
  void CreateSnapshotFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      CreateSnapshotFileCallback callback) override;
};

}  // namespace internal
}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_PROVIDER_ASYNC_FILE_UTIL_H_
