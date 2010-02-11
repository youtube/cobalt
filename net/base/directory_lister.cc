// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "net/base/net_errors.h"

namespace net {

static const int kFilesPerEvent = 8;

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
  std::vector<file_util::FileEnumerator::FindInfo> data;
  int error;
};

// Comparator for sorting FindInfo's. This uses the locale aware filename
// comparison function on the filenames for sorting in the user's locale.
static bool CompareFindInfo(const file_util::FileEnumerator::FindInfo& a,
                            const file_util::FileEnumerator::FindInfo& b) {
  // Parent directory before all else.
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(a)))
    return true;
  if (file_util::IsDotDot(file_util::FileEnumerator::GetFilename(b)))
    return false;

  // Directories before regular files.
  bool a_is_directory = file_util::FileEnumerator::IsDirectory(a);
  bool b_is_directory = file_util::FileEnumerator::IsDirectory(b);
  if (a_is_directory != b_is_directory)
    return a_is_directory;

  return file_util::LocaleAwareCompareFilenames(
      file_util::FileEnumerator::GetFilename(a),
      file_util::FileEnumerator::GetFilename(b));
}

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : dir_(dir),
      delegate_(delegate),
      message_loop_(NULL),
      thread_(kNullThreadHandle) {
  DCHECK(!dir.value().empty());
}

DirectoryLister::~DirectoryLister() {
  if (thread_) {
    PlatformThread::Join(thread_);
  }
}

bool DirectoryLister::Start() {
  // spawn a thread to enumerate the specified directory

  // pass events back to the current thread
  message_loop_ = MessageLoop::current();
  DCHECK(message_loop_) << "calling thread must have a message loop";

  AddRef();  // the thread will release us when it is done

  if (!PlatformThread::Create(0, this, &thread_)) {
    Release();
    return false;
  }

  return true;
}

void DirectoryLister::Cancel() {
  canceled_.Set();

  if (thread_) {
    PlatformThread::Join(thread_);
    thread_ = kNullThreadHandle;
  }
}

void DirectoryLister::ThreadMain() {
  DirectoryDataEvent* e = new DirectoryDataEvent(this);

  if (!file_util::DirectoryExists(dir_)) {
    e->error = net::ERR_FILE_NOT_FOUND;
    message_loop_->PostTask(FROM_HERE, e);
    Release();
    return;
  }

  file_util::FileEnumerator file_enum(dir_, false,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::INCLUDE_DOT_DOT));

  while (!canceled_.IsSet() && !(file_enum.Next().value().empty())) {
    e->data.push_back(file_util::FileEnumerator::FindInfo());
    file_enum.GetFindInfo(&e->data[e->data.size() - 1]);

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
    std::sort(e->data.begin(), e->data.end(), CompareFindInfo);

    message_loop_->PostTask(FROM_HERE, e);
    e = new DirectoryDataEvent(this);
  }

  // Notify done
  Release();
  message_loop_->PostTask(FROM_HERE, e);
}

void DirectoryLister::OnReceivedData(
    const file_util::FileEnumerator::FindInfo* data, int count) {
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
    error = net::ERR_ABORTED;

  if (delegate_)
    delegate_->OnListDone(error);
}

}  // namespace net
