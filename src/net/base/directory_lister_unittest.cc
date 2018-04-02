// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class ListerDelegate : public DirectoryLister::DirectoryListerDelegate {
 public:
  ListerDelegate(bool recursive,
                 bool quit_loop_after_each_file)
      : error_(-1),
        recursive_(recursive),
        quit_loop_after_each_file_(quit_loop_after_each_file) {
  }

  void OnListFile(const DirectoryLister::DirectoryListerData& data) {
    file_list_.push_back(data.info);
    paths_.push_back(data.path);
    if (quit_loop_after_each_file_)
      MessageLoop::current()->Quit();
  }

  void OnListDone(int error) {
    error_ = error;
    MessageLoop::current()->Quit();
    if (recursive_)
      CheckRecursiveSort();
    else
      CheckSort();
  }

  void CheckRecursiveSort() {
    // Check that we got files in the right order.
    if (!file_list_.empty()) {
      for (size_t previous = 0, current = 1;
           current < file_list_.size();
           previous++, current++) {
        EXPECT_TRUE(file_util::LocaleAwareCompareFilenames(
            paths_[previous], paths_[current]));
      }
    }
  }

  void CheckSort() {
    // Check that we got files in the right order.
    if (!file_list_.empty()) {
      for (size_t previous = 0, current = 1;
           current < file_list_.size();
           previous++, current++) {
        // Directories should come before files.
        if (file_util::FileEnumerator::IsDirectory(file_list_[previous]) &&
            !file_util::FileEnumerator::IsDirectory(file_list_[current])) {
          continue;
        }
        EXPECT_FALSE(file_util::IsDotDot(
            file_util::FileEnumerator::GetFilename(file_list_[current])));
        EXPECT_EQ(file_util::FileEnumerator::IsDirectory(file_list_[previous]),
                  file_util::FileEnumerator::IsDirectory(file_list_[current]));
        EXPECT_TRUE(file_util::LocaleAwareCompareFilenames(
            file_util::FileEnumerator::GetFilename(file_list_[previous]),
            file_util::FileEnumerator::GetFilename(file_list_[current])));
      }
    }
  }

  int error() const { return error_; }

  int num_files() const { return file_list_.size(); }

 private:
  int error_;
  bool recursive_;
  bool quit_loop_after_each_file_;
  std::vector<file_util::FileEnumerator::FindInfo> file_list_;
  std::vector<FilePath> paths_;
};

TEST(DirectoryListerTest, BigDirTest) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));

  ListerDelegate delegate(false, false);
  DirectoryLister lister(path, &delegate);
  lister.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(OK, delegate.error());
}

TEST(DirectoryListerTest, BigDirRecursiveTest) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &path));

  ListerDelegate delegate(true, false);
  DirectoryLister lister(path, true, DirectoryLister::FULL_PATH, &delegate);
  lister.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(OK, delegate.error());
}

TEST(DirectoryListerTest, CancelTest) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));

  ListerDelegate delegate(false, true);
  DirectoryLister lister(path, &delegate);
  lister.Start();

  MessageLoop::current()->Run();

  int num_files = delegate.num_files();

  lister.Cancel();

  MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(num_files, delegate.num_files());
}

TEST(DirectoryListerTest, EmptyDirTest) {
  base::ScopedTempDir tempDir;
  EXPECT_TRUE(tempDir.CreateUniqueTempDir());

  bool kRecursive = false;
  bool kQuitLoopAfterEachFile = false;
  ListerDelegate delegate(kRecursive, kQuitLoopAfterEachFile);
  DirectoryLister lister(tempDir.path(), &delegate);
  lister.Start();

  MessageLoop::current()->Run();

  // Contains only the parent directory ("..")
  EXPECT_EQ(1, delegate.num_files());
  EXPECT_EQ(OK, delegate.error());
}

}  // namespace net
