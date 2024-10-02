// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_WRAPPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/drive/drive_uploader.h"

namespace sync_file_system {
namespace drive_backend {

// This class wraps a part of DriveUploaderInterface class to support weak
// pointer.  Each method wraps corresponding name method of
// DriveUploaderInterface.  See comments in drive_uploader_interface.h
// for details.
class DriveUploaderWrapper
    : public base::SupportsWeakPtr<DriveUploaderWrapper> {
 public:
  explicit DriveUploaderWrapper(drive::DriveUploaderInterface* drive_uploader);

  DriveUploaderWrapper(const DriveUploaderWrapper&) = delete;
  DriveUploaderWrapper& operator=(const DriveUploaderWrapper&) = delete;

  void UploadExistingFile(const std::string& resource_id,
                          const base::FilePath& local_file_path,
                          const std::string& content_type,
                          const drive::UploadExistingFileOptions& options,
                          drive::UploadCompletionCallback callback);

  void UploadNewFile(const std::string& parent_resource_id,
                     const base::FilePath& local_file_path,
                     const std::string& title,
                     const std::string& content_type,
                     const drive::UploadNewFileOptions& options,
                     drive::UploadCompletionCallback callback);

 private:
  raw_ptr<drive::DriveUploaderInterface> drive_uploader_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_WRAPPER_H_
