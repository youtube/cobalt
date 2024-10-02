// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "components/file_access/scoped_file_access.h"

namespace file_access {
// static
ScopedFileAccessDelegate* ScopedFileAccessDelegate::Get() {
  return scoped_file_access_delegate_;
}

// static
bool ScopedFileAccessDelegate::HasInstance() {
  return scoped_file_access_delegate_;
}

// static
void ScopedFileAccessDelegate::DeleteInstance() {
  if (scoped_file_access_delegate_) {
    delete scoped_file_access_delegate_;
    scoped_file_access_delegate_ = nullptr;
  }
}

// static
void ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(ScopedFileAccess)> callback) {
  if (request_files_access_for_system_io_callback_) {
    request_files_access_for_system_io_callback_->Run(files,
                                                      std::move(callback));
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

// static
ScopedFileAccessDelegate::RequestFilesAccessIOCallback
ScopedFileAccessDelegate::GetCallbackForSystem() {
  return base::BindRepeating(
      [](const std::vector<base::FilePath>& file_paths,
         base::OnceCallback<void(ScopedFileAccess)> callback) {
        if (request_files_access_for_system_io_callback_) {
          request_files_access_for_system_io_callback_->Run(
              file_paths, std::move(callback));
        } else {
          std::move(callback).Run(ScopedFileAccess::Allowed());
        }
      });
}

ScopedFileAccessDelegate::ScopedFileAccessDelegate() {
  if (scoped_file_access_delegate_) {
    delete scoped_file_access_delegate_;
  }
  scoped_file_access_delegate_ = this;
}

ScopedFileAccessDelegate::~ScopedFileAccessDelegate() {
  if (scoped_file_access_delegate_ == this) {
    scoped_file_access_delegate_ = nullptr;
  }
}

// static
ScopedFileAccessDelegate*
    ScopedFileAccessDelegate::scoped_file_access_delegate_ = nullptr;

// static
ScopedFileAccessDelegate::RequestFilesAccessIOCallback*
    ScopedFileAccessDelegate::request_files_access_for_system_io_callback_ =
        nullptr;

ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    ScopedRequestFilesAccessCallbackForTesting(
        RequestFilesAccessIOCallback callback,
        bool restore_original_callback)
    : restore_original_callback_(restore_original_callback) {
  original_callback_ = request_files_access_for_system_io_callback_;
  request_files_access_for_system_io_callback_ =
      new RequestFilesAccessIOCallback(std::move(callback));
}

ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    ~ScopedRequestFilesAccessCallbackForTesting() {
  if (request_files_access_for_system_io_callback_) {
    delete request_files_access_for_system_io_callback_;
  }
  if (!restore_original_callback_ && original_callback_) {
    delete original_callback_;
    original_callback_ = nullptr;
  }
  request_files_access_for_system_io_callback_ = original_callback_;
}

void ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    RunOriginalCallback(
        const std::vector<base::FilePath>& path,
        base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  original_callback_->Run(path, std::move(callback));
}

}  // namespace file_access
