// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
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

void SortData(std::vector<DirectoryLister::DirectoryListerData>* data,
              DirectoryLister::SortType sort_type) {
  // Sort the results. See the TODO below (this sort should be removed and we
  // should do it from JS).
  if (sort_type == DirectoryLister::DATE)
    std::sort(data->begin(), data->end(), CompareDate);
  else if (sort_type == DirectoryLister::FULL_PATH)
    std::sort(data->begin(), data->end(), CompareFullPath);
  else if (sort_type == DirectoryLister::ALPHA_DIRS_FIRST)
    std::sort(data->begin(), data->end(), CompareAlphaDirsFirst);
  else
    DCHECK_EQ(DirectoryLister::NO_SORT, sort_type);
}

}  // namespace

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
  origin_loop_ = base::MessageLoopProxy::current();

  return base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&Core::StartInternal, this), true);
}

void DirectoryLister::Core::Cancel() {
  lister_ = NULL;
}

void DirectoryLister::Core::StartInternal() {

  if (!file_util::DirectoryExists(dir_)) {
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DirectoryLister::Core::OnDone, this, ERR_FILE_NOT_FOUND));
    return;
  }

  int types = file_util::FileEnumerator::FILES |
              file_util::FileEnumerator::DIRECTORIES;
  if (!recursive_)
    types |= file_util::FileEnumerator::INCLUDE_DOT_DOT;

  file_util::FileEnumerator file_enum(dir_, recursive_, types);

  FilePath path;
  std::vector<DirectoryListerData> file_data;
  while (lister_ && !(path = file_enum.Next()).empty()) {
    DirectoryListerData data;
    file_enum.GetFindInfo(&data.info);
    data.path = path;
    file_data.push_back(data);

    /* TODO(brettw) bug 24107: It would be nice to send incremental updates.
       We gather them all so they can be sorted, but eventually the sorting
       should be done from JS to give more flexibility in the page. When we do
       that, we can uncomment this to send incremental updates to the page.

    if (file_data.size() < kFilesPerEvent)
      continue;

    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DirectoryLister::Core::SendData, file_data));
    file_data.clear();
    */
  }

  SortData(&file_data, sort_);
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DirectoryLister::Core::SendData, this, file_data));

  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DirectoryLister::Core::OnDone, this, OK));
}

void DirectoryLister::Core::SendData(
    const std::vector<DirectoryLister::DirectoryListerData>& data) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  // We need to check for cancellation (indicated by NULL'ing of |lister_|)
  // which can happen during each callback.
  for (size_t i = 0; lister_ && i < data.size(); ++i)
    lister_->OnReceivedData(data[i]);
}

void DirectoryLister::Core::OnDone(int error) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
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
