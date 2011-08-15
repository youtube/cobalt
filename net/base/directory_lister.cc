// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

const int kFilesPerEvent = 8;

// Comparator for sorting lister results. This uses the locale aware filename
// comparison function on the filenames for sorting in the user's locale.
// Static.
bool CompareAlphaDirsFirst(const DirectoryLister::DirectoryListerData& a,
                           const DirectoryLister::DirectoryListerData& b) {
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

bool CompareDate(const DirectoryLister::DirectoryListerData& a,
                 const DirectoryLister::DirectoryListerData& b) {
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
bool CompareFullPath(const DirectoryLister::DirectoryListerData& a,
                     const DirectoryLister::DirectoryListerData& b) {
  return file_util::LocaleAwareCompareFilenames(a.path, b.path);
}

}  // namespace

// A task which is used to signal the delegate asynchronously.
class DirectoryLister::Core::DataEvent : public Task {
public:
  explicit DataEvent(Core* core) : core_(core), error_(OK) {
    // Allocations of the FindInfo aren't super cheap, so reserve space.
    data_.reserve(64);
  }

  virtual void Run() {
    DCHECK(core_->origin_loop_->BelongsToCurrentThread());
    if (data_.empty()) {
      core_->OnDone(error_);
      return;
    }
    core_->OnReceivedData(&data_[0], static_cast<int>(data_.size()));
  }

  void set_error(int error) { error_ = error; }

  void AddData(const DirectoryListerData& data) {
    data_.push_back(data);
  }

  bool HasData() const {
    return !data_.empty();
  }

  void SortData(SortType sort_type) {
    // Sort the results. See the TODO below (this sort should be removed and we
    // should do it from JS).
    if (sort_type == DATE)
      std::sort(data_.begin(), data_.end(), CompareDate);
    else if (sort_type == FULL_PATH)
      std::sort(data_.begin(), data_.end(), CompareFullPath);
    else if (sort_type == ALPHA_DIRS_FIRST)
      std::sort(data_.begin(), data_.end(), CompareAlphaDirsFirst);
    else
      DCHECK_EQ(NO_SORT, sort_type);
  }

 private:
  scoped_refptr<Core> core_;
  std::vector<DirectoryListerData> data_;
  int error_;
};

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        core_(new Core(dir, false, ALPHA_DIRS_FIRST, this))),
      delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!dir.value().empty());
}

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 bool recursive,
                                 SortType sort,
                                 DirectoryListerDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        core_(new Core(dir, recursive, sort, this))),
      delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!dir.value().empty());
}

DirectoryLister::~DirectoryLister() {
  Cancel();
}

bool DirectoryLister::Start() {
  return core_->Start();
}

void DirectoryLister::Cancel() {
  return core_->Cancel();
}

DirectoryLister::Core::Core(const FilePath& dir,
                            bool recursive,
                            SortType sort,
                            DirectoryLister* lister)
    : dir_(dir),
      recursive_(recursive),
      sort_(sort),
      lister_(lister) {
  DCHECK(lister_);
}

DirectoryLister::Core::~Core() {}

bool DirectoryLister::Core::Start() {
  origin_loop_ = base::MessageLoopProxy::CreateForCurrentThread();

  return base::WorkerPool::PostTask(
      FROM_HERE, NewRunnableMethod(this, &Core::StartInternal), true);
}

void DirectoryLister::Core::Cancel() {
  lister_ = NULL;
}

void DirectoryLister::Core::StartInternal() {
  DataEvent* e = new DataEvent(this);

  if (!file_util::DirectoryExists(dir_)) {
    e->set_error(ERR_FILE_NOT_FOUND);
    origin_loop_->PostTask(FROM_HERE, e);
    return;
  }

  int types = file_util::FileEnumerator::FILES |
              file_util::FileEnumerator::DIRECTORIES;
  if (!recursive_)
    types |= file_util::FileEnumerator::INCLUDE_DOT_DOT;

  file_util::FileEnumerator file_enum(dir_, recursive_,
      static_cast<file_util::FileEnumerator::FileType>(types));

  FilePath path;
  while (lister_ && !(path = file_enum.Next()).empty()) {
    DirectoryListerData data;
    file_enum.GetFindInfo(&data.info);
    data.path = path;
    e->AddData(data);

    /* TODO(brettw) bug 24107: It would be nice to send incremental updates.
       We gather them all so they can be sorted, but eventually the sorting
       should be done from JS to give more flexibility in the page. When we do
       that, we can uncomment this to send incremental updates to the page.
    if (++e->count == kFilesPerEvent) {
      message_loop_->PostTask(FROM_HERE, e);
      e = new DataEvent(this);
    }
    */
  }

  if (e->HasData()) {
    e->SortData(sort_);
    origin_loop_->PostTask(FROM_HERE, e);
    e = new DataEvent(this);
  }

  // Notify done
  origin_loop_->PostTask(FROM_HERE, e);
}

void DirectoryLister::Core::OnReceivedData(const DirectoryListerData* data,
                                           int count) {
  // We need to check for cancellation (indicated by NULL'ing of |lister_|),
  // which can happen during each callback.
  for (int i = 0; lister_ && i < count; ++i)
    lister_->OnReceivedData(data[i]);
}

void DirectoryLister::Core::OnDone(int error) {
  if (lister_)
    lister_->OnDone(error);
}

void DirectoryLister::OnReceivedData(const DirectoryListerData& data) {
  delegate_->OnListFile(data);
}

void DirectoryLister::OnDone(int error) {
  delegate_->OnListDone(error);
}

}  // namespace net
