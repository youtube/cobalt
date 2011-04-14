// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for Unix systems which don't have the inotify headers, and
// thus cannot build file_watcher_inotify.cc

#include "base/files/file_path_watcher.h"

namespace base {
namespace files {

namespace {

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  virtual bool Watch(const FilePath& path,
                     FileWatcher::Delegate* delegate,
                     base::MessageLoopProxy*) OVERRIDE {
    return false;
  }
};

}  // namespace

FilePathWatcher::FilePathWatcher() {
  impl_ = new FilePathWatcherImpl();
}

}  // namespace files
}  // namespace base
