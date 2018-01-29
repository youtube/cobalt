// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cross platform methods for FilePathWatcher. See the various platform
// specific implementation files, too.

#include "base/files/file_path_watcher.h"

#include "base/logging.h"
#include "base/message_loop.h"

namespace base {
namespace files {

namespace {

// A delegate implementation for the callback interface.
class FilePathWatcherDelegate : public base::files::FilePathWatcher::Delegate {
 public:
  explicit FilePathWatcherDelegate(const FilePathWatcher::Callback& callback)
      : callback_(callback) {}

  // FilePathWatcher::Delegate implementation.
  virtual void OnFilePathChanged(const FilePath& path) override {
    callback_.Run(path, false);
  }

  virtual void OnFilePathError(const FilePath& path) override {
    callback_.Run(path, true);
  }

 private:
  virtual ~FilePathWatcherDelegate() {}

  FilePathWatcher::Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherDelegate);
};

}  // namespace

FilePathWatcher::~FilePathWatcher() {
  impl_->Cancel();
}

// static
void FilePathWatcher::CancelWatch(
    const scoped_refptr<PlatformDelegate>& delegate) {
  delegate->CancelOnMessageLoopThread();
}

bool FilePathWatcher::Watch(const FilePath& path, Delegate* delegate) {
  DCHECK(path.IsAbsolute());
  return impl_->Watch(path, false, delegate);
}

FilePathWatcher::PlatformDelegate::PlatformDelegate(): cancelled_(false) {
}

FilePathWatcher::PlatformDelegate::~PlatformDelegate() {
  DCHECK(is_cancelled());
}

bool FilePathWatcher::Watch(const FilePath& path,
                            bool recursive,
                            const Callback& callback) {
  DCHECK(path.IsAbsolute());
  return impl_->Watch(path, recursive, new FilePathWatcherDelegate(callback));
}

}  // namespace files
}  // namespace base
