// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DIRECTORY_LISTER_H_
#define NET_BASE_DIRECTORY_LISTER_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"

class MessageLoop;

namespace net {

//
// This class provides an API for listing the contents of a directory on the
// filesystem asynchronously.  It spawns a background thread, and enumerates
// the specified directory on that thread.  It marshalls WIN32_FIND_DATA
// structs over to the main application thread.  The consumer of this class
// is insulated from any of the multi-threading details.
//
class DirectoryLister : public base::RefCountedThreadSafe<DirectoryLister>,
                        public base::PlatformThread::Delegate {
 public:
  // Represents one file found.
  struct DirectoryListerData {
    file_util::FileEnumerator::FindInfo info;
    FilePath path;
  };

  // Implement this class to receive directory entries.
  class DirectoryListerDelegate {
   public:
    // Called for each file found by the lister.
    virtual void OnListFile(const DirectoryListerData& data) = 0;

    // Called when the listing is complete.
    virtual void OnListDone(int error) = 0;

   protected:
    virtual ~DirectoryListerDelegate() {}
  };

  // Sort options
  // ALPHA_DIRS_FIRST is the default sort :
  //   directories first in name order, then files by name order
  // FULL_PATH sorts by paths as strings, ignoring files v. directories
  // DATE sorts by last modified date
  enum SORT_TYPE {
    NO_SORT,
    DATE,
    ALPHA_DIRS_FIRST,
    FULL_PATH
  };

  DirectoryLister(const FilePath& dir,
                  DirectoryListerDelegate* delegate);

  DirectoryLister(const FilePath& dir,
                  bool recursive,
                  SORT_TYPE sort,
                  DirectoryListerDelegate* delegate);


  // Call this method to start the directory enumeration thread.
  bool Start();

  // Call this method to asynchronously stop directory enumeration.  The
  // delegate will receive the OnListDone notification with an error code of
  // ERR_ABORTED.
  void Cancel();

  // The delegate pointer may be modified at any time.
  DirectoryListerDelegate* delegate() const { return delegate_; }
  void set_delegate(DirectoryListerDelegate* d) { delegate_ = d; }

  // PlatformThread::Delegate implementation
  virtual void ThreadMain();

 private:
  friend class base::RefCountedThreadSafe<DirectoryLister>;
  friend class DirectoryDataEvent;

  ~DirectoryLister();

  // Comparison methods for sorting, chosen based on |sort_|.
  static bool CompareAlphaDirsFirst(const DirectoryListerData& a,
                                    const DirectoryListerData& b);
  static bool CompareDate(const DirectoryListerData& a,
                          const DirectoryListerData& b);
  static bool CompareFullPath(const DirectoryListerData& a,
                              const DirectoryListerData& b);

  void OnReceivedData(const DirectoryListerData* data, int count);
  void OnDone(int error);

  FilePath dir_;
  bool recursive_;
  DirectoryListerDelegate* delegate_;
  SORT_TYPE sort_;
  MessageLoop* message_loop_;
  base::PlatformThreadHandle thread_;
  base::CancellationFlag canceled_;
};

}  // namespace net

#endif  // NET_BASE_DIRECTORY_LISTER_H_
