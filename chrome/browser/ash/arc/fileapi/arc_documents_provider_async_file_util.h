// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ASYNC_FILE_UTIL_H_

#include <memory>

#include "base/functional/callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace arc {

// The implementation of storage::AsyncFileUtil for documents providers.
//
// All of the methods must be called on the IO thread.
// Assuming that the documents-provider file system is used only by Files app,
// this implementation does not satisfy the definition of storage::AsyncFileUtil
// interface completely. We omit some details which Files app doesn't care.
// This is for simpicity, but results in layering violation. We should
// complement the implementations to prepare for general usage in the future.
// crbug.com/946329.
class ArcDocumentsProviderAsyncFileUtil : public storage::AsyncFileUtil {
 public:
  ArcDocumentsProviderAsyncFileUtil();

  ArcDocumentsProviderAsyncFileUtil(const ArcDocumentsProviderAsyncFileUtil&) =
      delete;
  ArcDocumentsProviderAsyncFileUtil& operator=(
      const ArcDocumentsProviderAsyncFileUtil&) = delete;

  ~ArcDocumentsProviderAsyncFileUtil() override;

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

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ASYNC_FILE_UTIL_H_
