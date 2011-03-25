// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"

namespace net {

static const int kFilesPerEvent = 8;

// A task which is used to signal the delegate asynchronously.
class DirectoryDataEvent : public Task {
public:
  explicit DirectoryDataEvent(DirectoryLister* d) : lister(d), error(0) {
    // Allocations of the FindInfo aren't super cheap, so reserve space.
    data.reserve(64);
  }

  void Run() {
    if (data.empty()) {
      lister->OnDone(error);
      return;
    }
    lister->OnReceivedData(&data[0], static_cast<int>(data.size()));
  }

  scoped_refptr<DirectoryLister> lister;
  std::vector<DirectoryLister::DirectoryListerData> data;
  int error;
};

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : dir_(dir),
      recursive_(false),
      delegate_(delegate),
      sort_(ALPHA_DIRS_FIRST),
      message_loop_(NULL),
      thread_(base::kNullThreadHandle) {
  DCHECK(!dir.value().empty());
}

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 bool recursive,
                                 SORT_TYPE sort,
                                 DirectoryListerDelegate* delegate)
    : dir_(dir),
      recursive_(recursive),
      delegate_(delegate),
      sort_(sort),
      message_loop_(NULL),
      thread_(base::kNullThreadHandle) {
  DCHECK(!dir.value().empty());
}

bool DirectoryLister::Start() {
  // spawn a thread to enumerate the specified directory

  // pass events back to the current thread
  message_loop_ = MessageLoop::current();
  DCHECK(message_loop_) << "calling thread must have a message loop";

  AddRef();  // the thread will release us when it is done

  if (!base::PlatformThread::Create(0, this, &thread_)) {
    Release();
    return false;
  }

  return true;
}

void DirectoryLister::Cancel() {
  canceled_.Set();

  if (thread_) {
    // This is a bug and we should stop joining this thread.
    // http://crbug.com/65331
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::PlatformThread::Join(thread_);
    thread_ = base::kNullThreadHandle;
  }
}

void DirectoryLister::ThreadMain() {
  DirectoryDataEvent* e = new DirectoryDataEvent(this);

  if (!file_util::DirectoryExists(dir_)) {
    e->error = ERR_FILE_NOT_FOUND;
    message_loop_->PostTask(FROM_HERE, e);
    Release();
    return;
  }

  int types = file_util::FileEnumerator::FILES |
              file_util::FileEnumerator::DIRECTORIES;
  if (!recursive_)
    types |= file_util::FileEnumerator::INCLUDE_DOT_DOT;

  file_util::FileEnumerator file_enum(dir_, recursive_,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(types));

  FilePath path;
  while (!canceled_.IsSet() && !(path = file_enum.Next()).empty()) {
    DirectoryListerData data;
    file_enum.GetFindInfo(&data.info);
    data.path = path;
    e->data.push_back(data);

    /* TODO(brettw) bug 24107: It would be nice to send incremental updates.
       We gather them all so they can be sorted, but eventually the sorting
       should be done from JS to give more flexibility in the page. When we do
       that, we can uncomment this to send incremental updates to the page.
    if (++e->count == kFilesPerEvent) {
      message_loop_->PostTask(FROM_HERE, e);
      e = new DirectoryDataEvent(this);
    }
    */
  }

  if (!e->data.empty()) {
    // Sort the results. See the TODO above (this sort should be removed and we
    // should do it from JS).
    if (sort_ == DATE)
      std::sort(e->data.begin(), e->data.end(), CompareDate);
    else if (sort_ == FULL_PATH)
      std::sort(e->data.begin(), e->data.end(), CompareFullPath);
    else if (sort_ == ALPHA_DIRS_FIRST)
      std::sort(e->data.begin(), e->data.end(), CompareAlphaDirsFirst);
    else
      DCHECK_EQ(NO_SORT, sort_);

    message_loop_->PostTask(FROM_HERE, e);
    e = new DirectoryDataEvent(this);
  }

  // Notify done
  Release();
  message_loop_->PostTask(FROM_HERE, e);
}

DirectoryLister::~DirectoryLister() {
  if (thread_) {
    // This is a bug and we should stop joining this thread.
    // http://crbug.com/65331
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::PlatformThread::Join(thread_);
  }
}

// Comparator for sorting lister results. This uses the locale aware filename
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool DirectoryLister::CompareAlphaDirsFirst(const DirectoryListerData& a,
                                            const DirectoryListerData& b) {
  // Parent directory before all else.
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(a.info)))
    return true;
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(b.info)))
    return false;

  // Directories before regular files.
  bool a_is_directory = file_util::FileEnumerator::IsDirectory(a.info);
  bool b_is_directory = file_util::FileEnumerator::IsDirectory(b.info);
  if (a_is_directory != b_is_directory)
    return a_is_directory;

  return file_util::LocaleAwareCompareFilenames(
      file_util::FileEnumerator::GetFilename(a.info),
      file_util::FileEnumerator::GetFilename(b.info));
}

// Static.
bool DirectoryLister::CompareDate(const DirectoryListerData& a,
                                  const DirectoryListerData& b) {
  // Parent directory before all else.
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(a.info)))
    return true;
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(b.info)))
    return false;

  // Directories before regular files.
  bool a_is_directory = file_util::FileEnumerator::IsDirectory(a.info);
  bool b_is_directory = file_util::FileEnumerator::IsDirectory(b.info);
  if (a_is_directory != b_is_directory)
    return a_is_directory;
#if defined(OS_POSIX)
  return a.info.stat.st_mtime > b.info.stat.st_mtime;
#elif defined(OS_WIN)
  if (a.info.ftLastWriteTime.dwHighDateTime ==
      b.info.ftLastWriteTime.dwHighDateTime) {
    return a.info.ftLastWriteTime.dwLowDateTime >
           b.info.ftLastWriteTime.dwLowDateTime;
  } else {
    return a.info.ftLastWriteTime.dwHighDateTime >
           b.info.ftLastWriteTime.dwHighDateTime;
  }
#endif
}

// Comparator for sorting find result by paths. This uses the locale-aware
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool DirectoryLister::CompareFullPath(const DirectoryListerData& a,
                                      const DirectoryListerData& b) {
  return file_util::LocaleAwareCompareFilenames(a.path, b.path);
}

void DirectoryLister::OnReceivedData(const DirectoryListerData* data,
                                     int count) {
  // Since the delegate can clear itself during the OnListFile callback, we
  // need to null check it during each iteration of the loop.  Similarly, it is
  // necessary to check the canceled_ flag to avoid sending data to a delegate
  // who doesn't want anymore.
  for (int i = 0; !canceled_.IsSet() && delegate_ && i < count; ++i)
    delegate_->OnListFile(data[i]);
}

void DirectoryLister::OnDone(int error) {
  // If canceled is set, we need to report some kind of error,
  // but don't overwrite the error condition if it is already set.
  if (!error && canceled_.IsSet())
    error = ERR_ABORTED;

  if (delegate_)
    delegate_->OnListDone(error);
}

}  // namespace net
