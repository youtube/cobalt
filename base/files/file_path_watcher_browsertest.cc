// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <set>

#if defined(OS_WIN)
#include <windows.h>
#include <aclapi.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#endif

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace files {

namespace {

class TestDelegate;

// Aggregates notifications from the test delegates and breaks the message loop
// the test thread is waiting on once they all came in.
class NotificationCollector
    : public base::RefCountedThreadSafe<NotificationCollector> {
 public:
  NotificationCollector()
      : loop_(base::MessageLoopProxy::current()) {}

  // Called from the file thread by the delegates.
  void OnChange(TestDelegate* delegate) {
    loop_->PostTask(FROM_HERE,
                    base::Bind(&NotificationCollector::RecordChange, this,
                               make_scoped_refptr(delegate)));
  }

  void Register(TestDelegate* delegate) {
    delegates_.insert(delegate);
  }

  void Reset() {
    signaled_.clear();
  }

  bool Success() {
    return signaled_ == delegates_;
  }

 private:
  void RecordChange(TestDelegate* delegate) {
    ASSERT_TRUE(loop_->BelongsToCurrentThread());
    ASSERT_TRUE(delegates_.count(delegate));
    signaled_.insert(delegate);

    // Check whether all delegates have been signaled.
    if (signaled_ == delegates_)
      loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  // Set of registered delegates.
  std::set<TestDelegate*> delegates_;

  // Set of signaled delegates.
  std::set<TestDelegate*> signaled_;

  // The loop we should break after all delegates signaled.
  scoped_refptr<base::MessageLoopProxy> loop_;
};

// A mock FilePathWatcher::Delegate for testing. I'd rather use gmock, but it's
// not thread safe for setting expectations, so the test code couldn't safely
// reset expectations while the file watcher is running. In order to allow this,
// we keep simple thread safe status flags in TestDelegate.
class TestDelegate : public FilePathWatcher::Delegate {
 public:
  // The message loop specified by |loop| will be quit if a notification is
  // received while the delegate is |armed_|. Note that the testing code must
  // guarantee |loop| outlives the file thread on which OnFilePathChanged runs.
  explicit TestDelegate(NotificationCollector* collector)
      : collector_(collector) {
    collector_->Register(this);
  }

  virtual void OnFilePathChanged(const FilePath&) {
    collector_->OnChange(this);
  }

  virtual void OnFilePathError(const FilePath& path) {
    ADD_FAILURE() << "Error " << path.value();
  }

 private:
  scoped_refptr<NotificationCollector> collector_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

void SetupWatchCallback(const FilePath& target,
                        FilePathWatcher* watcher,
                        FilePathWatcher::Delegate* delegate,
                        bool* result,
                        base::WaitableEvent* completion) {
  *result = watcher->Watch(target, delegate);
  completion->Signal();
}

class FilePathWatcherTest : public testing::Test {
 public:
  FilePathWatcherTest()
      : file_thread_("FilePathWatcherTest") {}

  virtual ~FilePathWatcherTest() {}

 protected:
  virtual void SetUp() {
    // Create a separate file thread in order to test proper thread usage.
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    ASSERT_TRUE(file_thread_.StartWithOptions(options));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    collector_ = new NotificationCollector();
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  FilePath test_file() {
    return temp_dir_.path().AppendASCII("FilePathWatcherTest");
  }

  FilePath test_link() {
    return temp_dir_.path().AppendASCII("FilePathWatcherTest.lnk");
  }

  // Write |content| to |file|. Returns true on success.
  bool WriteFile(const FilePath& file, const std::string& content) {
    int write_size = file_util::WriteFile(file, content.c_str(),
                                          content.length());
    return write_size == static_cast<int>(content.length());
  }

  bool SetupWatch(const FilePath& target,
                  FilePathWatcher* watcher,
                  FilePathWatcher::Delegate* delegate) WARN_UNUSED_RESULT {
    base::WaitableEvent completion(false, false);
    bool result;
    file_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(SetupWatchCallback, target, watcher,
                   make_scoped_refptr(delegate), &result, &completion));
    completion.Wait();
    return result;
  }

  bool WaitForEvents() WARN_UNUSED_RESULT {
    collector_->Reset();
    loop_.Run();
    return collector_->Success();
  }

  NotificationCollector* collector() { return collector_.get(); }

  MessageLoop loop_;
  base::Thread file_thread_;
  ScopedTempDir temp_dir_;
  scoped_refptr<NotificationCollector> collector_;
};

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, NewFile) {
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, ModifiedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that moving the file into place is caught.
TEST_F(FilePathWatcherTest, MovedFile) {
  FilePath source_file(temp_dir_.path().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(file_util::Move(source_file, test_file()));
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  // Now make sure we get notified if the file is deleted.
  file_util::Delete(test_file(), false);
  ASSERT_TRUE(WaitForEvents());
}

// Used by the DeleteDuringNotify test below.
// Deletes the FilePathWatcher when it's notified.
class Deleter : public FilePathWatcher::Delegate {
 public:
  Deleter(FilePathWatcher* watcher, MessageLoop* loop)
      : watcher_(watcher),
        loop_(loop) {
  }

  virtual void OnFilePathChanged(const FilePath& path) {
    watcher_.reset();
    loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  scoped_ptr<FilePathWatcher> watcher_;
  MessageLoop* loop_;
};

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  FilePathWatcher* watcher = new FilePathWatcher;
  // Takes ownership of watcher.
  scoped_refptr<Deleter> deleter(new Deleter(watcher, &loop_));
  ASSERT_TRUE(SetupWatch(test_file(), watcher, deleter.get()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter->watcher_.get() == NULL);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
// Flaky on MacOS. http://crbug.com/85930
#if defined(OS_MACOSX)
#define MAYBE_DestroyWithPendingNotification FLAKY_DestroyWithPendingNotification
#else
#define MAYBE_DestroyWithPendingNotification DestroyWithPendingNotification
#endif
TEST_F(FilePathWatcherTest, MAYBE_DestroyWithPendingNotification) {
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  FilePathWatcher* watcher = new FilePathWatcher;
  ASSERT_TRUE(SetupWatch(test_file(), watcher, delegate.get()));
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  file_thread_.message_loop_proxy()->DeleteSoon(FROM_HERE, watcher);
}

TEST_F(FilePathWatcherTest, MultipleWatchersSingleFile) {
  FilePathWatcher watcher1, watcher2;
  scoped_refptr<TestDelegate> delegate1(new TestDelegate(collector()));
  scoped_refptr<TestDelegate> delegate2(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher1, delegate1.get()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher2, delegate2.get()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get()));

  ASSERT_TRUE(file_util::CreateDirectory(dir));

  ASSERT_TRUE(WriteFile(file, "content"));

  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(file_util::Delete(file, false));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
TEST_F(FilePathWatcherTest, DirectoryChain) {
  FilePath path(temp_dir_.path());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(base::StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }

  FilePathWatcher watcher;
  FilePath file(path.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get()));

  FilePath sub_path(temp_dir_.path());
  for (std::vector<std::string>::const_iterator d(dir_names.begin());
       d != dir_names.end(); ++d) {
    sub_path = sub_path.AppendASCII(*d);
    ASSERT_TRUE(file_util::CreateDirectory(sub_path));
  }
  VLOG(1) << "Create File";
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
  ASSERT_TRUE(WaitForEvents());
}

#if defined(OS_MACOSX)
// http://crbug.com/85930
#define DisappearingDirectory FLAKY_DisappearingDirectory
#endif
TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &watcher, delegate.get()));

  ASSERT_TRUE(file_util::Delete(dir, true));
  ASSERT_TRUE(WaitForEvents());
}

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  ASSERT_TRUE(file_util::Delete(test_file(), false));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath file1(dir.AppendASCII("file1"));
  FilePath file2(dir.AppendASCII("file2"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dir, &watcher, delegate.get()));

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation";
  ASSERT_TRUE(WaitForEvents());

#if !defined(OS_MACOSX)
  // Mac implementation does not detect files modified in a directory.
  ASSERT_TRUE(WriteFile(file1, "content v2"));
  VLOG(1) << "Waiting for file1 modification";
  ASSERT_TRUE(WaitForEvents());
#endif  // !OS_MACOSX

  ASSERT_TRUE(file_util::Delete(file1, false));
  VLOG(1) << "Waiting for file1 deletion";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, MoveParent) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath dest(temp_dir_.path().AppendASCII("dest"));
  FilePath subdir(dir.AppendASCII("subdir"));
  FilePath file(subdir.AppendASCII("file"));
  scoped_refptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(file, &file_watcher, file_delegate.get()));
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(subdir, &subdir_watcher, subdir_delegate.get()));

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(subdir));
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());

  // Move the parent directory.
  file_util::Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  ASSERT_TRUE(WaitForEvents());
}

TEST_F(FilePathWatcherTest, MoveChild) {
  FilePathWatcher file_watcher;
  FilePathWatcher subdir_watcher;
  FilePath source_dir(temp_dir_.path().AppendASCII("source"));
  FilePath source_subdir(source_dir.AppendASCII("subdir"));
  FilePath source_file(source_subdir.AppendASCII("file"));
  FilePath dest_dir(temp_dir_.path().AppendASCII("dest"));
  FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  FilePath dest_file(dest_subdir.AppendASCII("file"));

  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  scoped_refptr<TestDelegate> file_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dest_file, &file_watcher, file_delegate.get()));
  scoped_refptr<TestDelegate> subdir_delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(dest_subdir, &subdir_watcher, subdir_delegate.get()));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(file_util::Move(source_dir, dest_dir));
  ASSERT_TRUE(WaitForEvents());
}

#if !defined(OS_LINUX)
// Linux implementation of FilePathWatcher doesn't catch attribute changes.
// http://crbug.com/78043

// Verify that changing attributes on a file is caught
TEST_F(FilePathWatcherTest, FileAttributesChanged) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, delegate.get()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(file_util::MakeFileUnreadable(test_file()));
  ASSERT_TRUE(WaitForEvents());
}

#endif  // !OS_LINUX

#if defined(OS_LINUX)

// Verify that creating a symlink is caught.
TEST_F(FilePathWatcherTest, CreateLink) {
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get()));

  // Now make sure we get notified if the link is created.
  // Note that test_file() doesn't have to exist.
  ASSERT_TRUE(file_util::CreateSymbolicLink(test_file(), test_link()));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that deleting a symlink is caught.
TEST_F(FilePathWatcherTest, DeleteLink) {
  // Unfortunately this test case only works if the link target exists.
  // TODO(craig) fix this as part of crbug.com/91561.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(file_util::CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get()));

  // Now make sure we get notified if the link is deleted.
  ASSERT_TRUE(file_util::Delete(test_link(), false));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that modifying a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, ModifiedLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(file_util::CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that creating a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, CreateTargetLinkedFile) {
  ASSERT_TRUE(file_util::CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get()));

  // Now make sure we get notified if the target file is created.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that deleting a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, DeleteTargetLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(file_util::CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, delegate.get()));

  // Now make sure we get notified if the target file is deleted.
  ASSERT_TRUE(file_util::Delete(test_file(), false));
  ASSERT_TRUE(WaitForEvents());
}

// Verify that watching a file whose parent directory is a link that
// doesn't exist yet works if the symlink is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart1) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.path().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // dir/file should exist.
  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  // Note that we are watching dir.lnk/file which doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get()));

  ASSERT_TRUE(file_util::CreateSymbolicLink(dir, link_dir));
  VLOG(1) << "Waiting for link creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(file_util::Delete(file, false));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
}

// Verify that watching a file whose parent directory is a
// dangling symlink works if the directory is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart2) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.path().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  // Now create the link from dir.lnk pointing to dir but
  // neither dir nor dir/file exist yet.
  ASSERT_TRUE(file_util::CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get()));

  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for dir/file creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(file_util::Delete(file, false));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
}

// Verify that watching a file with a symlink on the path
// to the file works.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart3) {
  FilePathWatcher watcher;
  FilePath dir(temp_dir_.path().AppendASCII("dir"));
  FilePath link_dir(temp_dir_.path().AppendASCII("dir.lnk"));
  FilePath file(dir.AppendASCII("file"));
  FilePath linkfile(link_dir.AppendASCII("file"));
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(file_util::CreateDirectory(dir));
  ASSERT_TRUE(file_util::CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file but the file doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, delegate.get()));

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  ASSERT_TRUE(WaitForEvents());

  ASSERT_TRUE(file_util::Delete(file, false));
  VLOG(1) << "Waiting for file deletion";
  ASSERT_TRUE(WaitForEvents());
}

#endif  // OS_LINUX

enum Permission {
  Read,
  Write,
  Execute
};

bool ChangeFilePermissions(const FilePath& path, Permission perm, bool allow) {
#if defined(OS_POSIX)
  struct stat stat_buf;

  if (stat(path.value().c_str(), &stat_buf) != 0)
    return false;

  mode_t mode = 0;
  switch (perm) {
    case Read:
      mode = S_IRUSR | S_IRGRP | S_IROTH;
      break;
    case Write:
      mode = S_IWUSR | S_IWGRP | S_IWOTH;
      break;
    case Execute:
      mode = S_IXUSR | S_IXGRP | S_IXOTH;
      break;
    default:
      ADD_FAILURE() << "unknown perm " << perm;
      return false;
  }
  if (allow) {
    stat_buf.st_mode |= mode;
  } else {
    stat_buf.st_mode &= ~mode;
  }
  return chmod(path.value().c_str(), stat_buf.st_mode) == 0;

#elif defined(OS_WIN)
  PACL old_dacl;
  PSECURITY_DESCRIPTOR security_descriptor;
  if (GetNamedSecurityInfo(const_cast<wchar_t*>(path.value().c_str()),
                           SE_FILE_OBJECT,
                           DACL_SECURITY_INFORMATION, NULL, NULL, &old_dacl,
                           NULL, &security_descriptor) != ERROR_SUCCESS)
    return false;

  DWORD mode = 0;
  switch (perm) {
    case Read:
      mode = GENERIC_READ;
      break;
    case Write:
      mode = GENERIC_WRITE;
      break;
    case Execute:
      mode = GENERIC_EXECUTE;
      break;
    default:
      ADD_FAILURE() << "unknown perm " << perm;
      return false;
  }

  // Deny Read access for the current user.
  EXPLICIT_ACCESS change;
  change.grfAccessPermissions = mode;
  change.grfAccessMode = allow ? GRANT_ACCESS : DENY_ACCESS;
  change.grfInheritance = 0;
  change.Trustee.pMultipleTrustee = NULL;
  change.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  change.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
  change.Trustee.TrusteeType = TRUSTEE_IS_USER;
  change.Trustee.ptstrName = L"CURRENT_USER";

  PACL new_dacl;
  if (SetEntriesInAcl(1, &change, old_dacl, &new_dacl) != ERROR_SUCCESS) {
    LocalFree(security_descriptor);
    return false;
  }

  DWORD rc = SetNamedSecurityInfo(const_cast<wchar_t*>(path.value().c_str()),
                                  SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  NULL, NULL, new_dacl, NULL);
  LocalFree(security_descriptor);
  LocalFree(new_dacl);

  return rc == ERROR_SUCCESS;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

#if defined(OS_MACOSX)
// Linux implementation of FilePathWatcher doesn't catch attribute changes.
// http://crbug.com/78043
// Windows implementation of FilePathWatcher catches attribute changes that
// don't affect the path being watched.
// http://crbug.com/78045

// Verify that changing attributes on a directory works.
TEST_F(FilePathWatcherTest, DirAttributesChanged) {
  FilePath test_dir1(temp_dir_.path().AppendASCII("DirAttributesChangedDir1"));
  FilePath test_dir2(test_dir1.AppendASCII("DirAttributesChangedDir2"));
  FilePath test_file(test_dir2.AppendASCII("DirAttributesChangedFile"));
  // Setup a directory hierarchy.
  ASSERT_TRUE(file_util::CreateDirectory(test_dir1));
  ASSERT_TRUE(file_util::CreateDirectory(test_dir2));
  ASSERT_TRUE(WriteFile(test_file, "content"));

  FilePathWatcher watcher;
  scoped_refptr<TestDelegate> delegate(new TestDelegate(collector()));
  ASSERT_TRUE(SetupWatch(test_file, &watcher, delegate.get()));

  // We should not get notified in this case as it hasn't affected our ability
  // to access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, false));
  loop_.PostDelayedTask(FROM_HERE,
                        MessageLoop::QuitClosure(),
                        TestTimeouts::tiny_timeout_ms());
  ASSERT_FALSE(WaitForEvents());
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, true));

  // We should get notified in this case because filepathwatcher can no
  // longer access the file
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, false));
  ASSERT_TRUE(WaitForEvents());
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, true));
}

#endif  // OS_MACOSX
}  // namespace

}  // namespace files
}  // namespace base
