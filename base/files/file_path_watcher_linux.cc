// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path_watcher.h"

#include <errno.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/threading/thread.h"

namespace base {
namespace files {

namespace {

class FilePathWatcherImpl;

// Singleton to manage all inotify watches.
// TODO(tony): It would be nice if this wasn't a singleton.
// http://crbug.com/38174
class InotifyReader {
 public:
  typedef int Watch;  // Watch descriptor used by AddWatch and RemoveWatch.
  static const Watch kInvalidWatch = -1;

  // Watch directory |path| for changes. |watcher| will be notified on each
  // change. Returns kInvalidWatch on failure.
  Watch AddWatch(const FilePath& path, FilePathWatcherImpl* watcher);

  // Remove |watch|. Returns true on success.
  bool RemoveWatch(Watch watch, FilePathWatcherImpl* watcher);

  // Callback for InotifyReaderTask.
  void OnInotifyEvent(const inotify_event* event);

 private:
  friend struct ::base::DefaultLazyInstanceTraits<InotifyReader>;

  typedef std::set<FilePathWatcherImpl*> WatcherSet;

  InotifyReader();
  ~InotifyReader();

  // We keep track of which delegates want to be notified on which watches.
  base::hash_map<Watch, WatcherSet> watchers_;

  // Lock to protect watchers_.
  base::Lock lock_;

  // Separate thread on which we run blocking read for inotify events.
  base::Thread thread_;

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Use self-pipe trick to unblock select during shutdown.
  int shutdown_pipe_[2];

  // Flag set to true when startup was successful.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReader);
};

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate,
                            public MessageLoop::DestructionObserver {
 public:
  FilePathWatcherImpl();

  // Called for each event coming from the watch. |fired_watch| identifies the
  // watch that fired, |child| indicates what has changed, and is relative to
  // the currently watched path for |fired_watch|. The flag |created| is true if
  // the object appears, and |is_directory| is set when the event refers to a
  // directory.
  void OnFilePathChanged(InotifyReader::Watch fired_watch,
                         const FilePath::StringType& child,
                         bool created,
                         bool is_directory);

  // Start watching |path| for changes and notify |delegate| on each change.
  // Returns true if watch for |path| has been added successfully.
  virtual bool Watch(const FilePath& path,
                     FilePathWatcher::Delegate* delegate) OVERRIDE;

  // Cancel the watch. This unregisters the instance with InotifyReader.
  virtual void Cancel() OVERRIDE;

  // Deletion of the FilePathWatcher will call Cancel() to dispose of this
  // object in the right thread. This also observes destruction of the required
  // cleanup thread, in case it quits before Cancel() is called.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 private:
  virtual ~FilePathWatcherImpl() {}

  // Cleans up and stops observing the |message_loop_| thread.
  void CancelOnMessageLoopThread() OVERRIDE;

  // Inotify watches are installed for all directory components of |target_|. A
  // WatchEntry instance holds the watch descriptor for a component and the
  // subdirectory for that identifies the next component.
  struct WatchEntry {
    WatchEntry(InotifyReader::Watch watch, const FilePath::StringType& subdir)
        : watch_(watch),
          subdir_(subdir) {}

    InotifyReader::Watch watch_;
    FilePath::StringType subdir_;
  };
  typedef std::vector<WatchEntry> WatchVector;

  // Reconfigure to watch for the most specific parent directory of |target_|
  // that exists. Updates |watched_path_|. Returns true on success.
  bool UpdateWatches() WARN_UNUSED_RESULT;

  // Delegate to notify upon changes.
  scoped_refptr<FilePathWatcher::Delegate> delegate_;

  // The file or directory we're supposed to watch.
  FilePath target_;

  // The vector of watches and next component names for all path components,
  // starting at the root directory. The last entry corresponds to the watch for
  // |target_| and always stores an empty next component name in |subdir_|.
  WatchVector watches_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherImpl);
};

class InotifyReaderTask : public Task {
 public:
  InotifyReaderTask(InotifyReader* reader, int inotify_fd, int shutdown_fd)
      : reader_(reader),
        inotify_fd_(inotify_fd),
        shutdown_fd_(shutdown_fd) {
  }

  virtual void Run() {
    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(inotify_fd_, &rfds);
      FD_SET(shutdown_fd_, &rfds);

      // Wait until some inotify events are available.
      int select_result =
        HANDLE_EINTR(select(std::max(inotify_fd_, shutdown_fd_) + 1,
                            &rfds, NULL, NULL, NULL));
      if (select_result < 0) {
        DPLOG(WARNING) << "select failed";
        return;
      }

      if (FD_ISSET(shutdown_fd_, &rfds))
        return;

      // Adjust buffer size to current event queue size.
      int buffer_size;
      int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD,
                                            &buffer_size));

      if (ioctl_result != 0) {
        DPLOG(WARNING) << "ioctl failed";
        return;
      }

      std::vector<char> buffer(buffer_size);

      ssize_t bytes_read = HANDLE_EINTR(read(inotify_fd_, &buffer[0],
                                             buffer_size));

      if (bytes_read < 0) {
        DPLOG(WARNING) << "read from inotify fd failed";
        return;
      }

      ssize_t i = 0;
      while (i < bytes_read) {
        inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        size_t event_size = sizeof(inotify_event) + event->len;
        DCHECK(i + event_size <= static_cast<size_t>(bytes_read));
        reader_->OnInotifyEvent(event);
        i += event_size;
      }
    }
  }

 private:
  InotifyReader* reader_;
  int inotify_fd_;
  int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReaderTask);
};

static base::LazyInstance<InotifyReader> g_inotify_reader(
    base::LINKER_INITIALIZED);

InotifyReader::InotifyReader()
    : thread_("inotify_reader"),
      inotify_fd_(inotify_init()),
      valid_(false) {
  shutdown_pipe_[0] = -1;
  shutdown_pipe_[1] = -1;
  if (inotify_fd_ >= 0 && pipe(shutdown_pipe_) == 0 && thread_.Start()) {
    thread_.message_loop()->PostTask(
        FROM_HERE, new InotifyReaderTask(this, inotify_fd_, shutdown_pipe_[0]));
    valid_ = true;
  }
}

InotifyReader::~InotifyReader() {
  if (valid_) {
    // Write to the self-pipe so that the select call in InotifyReaderTask
    // returns.
    ssize_t ret = HANDLE_EINTR(write(shutdown_pipe_[1], "", 1));
    DPCHECK(ret > 0);
    DCHECK_EQ(ret, 1);
    thread_.Stop();
  }
  if (inotify_fd_ >= 0)
    close(inotify_fd_);
  if (shutdown_pipe_[0] >= 0)
    close(shutdown_pipe_[0]);
  if (shutdown_pipe_[1] >= 0)
    close(shutdown_pipe_[1]);
}

InotifyReader::Watch InotifyReader::AddWatch(
    const FilePath& path, FilePathWatcherImpl* watcher) {
  if (!valid_)
    return kInvalidWatch;

  base::AutoLock auto_lock(lock_);

  Watch watch = inotify_add_watch(inotify_fd_, path.value().c_str(),
                                  IN_CREATE | IN_DELETE |
                                  IN_CLOSE_WRITE | IN_MOVE |
                                  IN_ONLYDIR);

  if (watch == kInvalidWatch)
    return kInvalidWatch;

  watchers_[watch].insert(watcher);

  return watch;
}

bool InotifyReader::RemoveWatch(Watch watch,
                                FilePathWatcherImpl* watcher) {
  if (!valid_)
    return false;

  base::AutoLock auto_lock(lock_);

  watchers_[watch].erase(watcher);

  if (watchers_[watch].empty()) {
    watchers_.erase(watch);
    return (inotify_rm_watch(inotify_fd_, watch) == 0);
  }

  return true;
}

void InotifyReader::OnInotifyEvent(const inotify_event* event) {
  if (event->mask & IN_IGNORED)
    return;

  FilePath::StringType child(event->len ? event->name : FILE_PATH_LITERAL(""));
  base::AutoLock auto_lock(lock_);

  for (WatcherSet::iterator watcher = watchers_[event->wd].begin();
       watcher != watchers_[event->wd].end();
       ++watcher) {
    (*watcher)->OnFilePathChanged(event->wd,
                                  child,
                                  event->mask & (IN_CREATE | IN_MOVED_TO),
                                  event->mask & IN_ISDIR);
  }
}

FilePathWatcherImpl::FilePathWatcherImpl()
    : delegate_(NULL) {
}

void FilePathWatcherImpl::OnFilePathChanged(
    InotifyReader::Watch fired_watch,
    const FilePath::StringType& child,
    bool created,
    bool is_directory) {

  if (!message_loop()->BelongsToCurrentThread()) {
    // Switch to message_loop_ to access watches_ safely.
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &FilePathWatcherImpl::OnFilePathChanged,
                          fired_watch,
                          child,
                          created,
                          is_directory));
    return;
  }

  DCHECK(MessageLoopForIO::current());

  // Find the entry in |watches_| that corresponds to |fired_watch|.
  WatchVector::const_iterator watch_entry(watches_.begin());
  for ( ; watch_entry != watches_.end(); ++watch_entry) {
    if (fired_watch == watch_entry->watch_)
      break;
  }

  // If this notification is from a previous generation of watches or the watch
  // has been cancelled (|watches_| is empty then), bail out.
  if (watch_entry == watches_.end())
    return;

  // Check whether a path component of |target_| changed.
  bool change_on_target_path = child.empty() || child == watch_entry->subdir_;

  // Check whether the change references |target_| or a direct child.
  DCHECK(watch_entry->subdir_.empty() || (watch_entry + 1) != watches_.end());
  bool target_changed = watch_entry->subdir_.empty() ||
      (watch_entry->subdir_ == child && (++watch_entry)->subdir_.empty());

  // Update watches if a directory component of the |target_| path (dis)appears.
  if (is_directory && change_on_target_path && !UpdateWatches()) {
    delegate_->OnFilePathError(target_);
    return;
  }

  // Report the following events:
  //  - The target or a direct child of the target got changed (in case the
  //    watched path refers to a directory).
  //  - One of the parent directories got moved or deleted, since the target
  //    disappears in this case.
  //  - One of the parent directories appears. The event corresponding to the
  //    target appearing might have been missed in this case, so recheck.
  if (target_changed ||
      (change_on_target_path && !created) ||
      (change_on_target_path && file_util::PathExists(target_))) {
    delegate_->OnFilePathChanged(target_);
  }
}

bool FilePathWatcherImpl::Watch(const FilePath& path,
                                FilePathWatcher::Delegate* delegate) {
  DCHECK(target_.empty());
  DCHECK(MessageLoopForIO::current());

  set_message_loop(base::MessageLoopProxy::CreateForCurrentThread());
  delegate_ = delegate;
  target_ = path;
  MessageLoop::current()->AddDestructionObserver(this);

  std::vector<FilePath::StringType> comps;
  target_.GetComponents(&comps);
  DCHECK(!comps.empty());
  for (std::vector<FilePath::StringType>::const_iterator comp(++comps.begin());
       comp != comps.end(); ++comp) {
    watches_.push_back(WatchEntry(InotifyReader::kInvalidWatch, *comp));
  }
  watches_.push_back(WatchEntry(InotifyReader::kInvalidWatch,
                                FilePath::StringType()));
  return UpdateWatches();
}

void FilePathWatcherImpl::Cancel() {
  if (!delegate_) {
    // Watch was never called, or the |message_loop_| thread is already gone.
    set_cancelled();
    return;
  }

  // Switch to the message_loop_ if necessary so we can access |watches_|.
  if (!message_loop()->BelongsToCurrentThread()) {
    message_loop()->PostTask(FROM_HERE,
                             new FilePathWatcher::CancelTask(this));
  } else {
    CancelOnMessageLoopThread();
  }
}

void FilePathWatcherImpl::CancelOnMessageLoopThread() {
  if (!is_cancelled()) {
    set_cancelled();
    MessageLoop::current()->RemoveDestructionObserver(this);

    for (WatchVector::iterator watch_entry(watches_.begin());
         watch_entry != watches_.end(); ++watch_entry) {
      if (watch_entry->watch_ != InotifyReader::kInvalidWatch)
        g_inotify_reader.Get().RemoveWatch(watch_entry->watch_, this);
    }
    watches_.clear();
    delegate_ = NULL;
    target_.clear();
  }
}

void FilePathWatcherImpl::WillDestroyCurrentMessageLoop() {
  CancelOnMessageLoopThread();
}

bool FilePathWatcherImpl::UpdateWatches() {
  // Ensure this runs on the message_loop_ exclusively in order to avoid
  // concurrency issues.
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Walk the list of watches and update them as we go.
  FilePath path(FILE_PATH_LITERAL("/"));
  bool path_valid = true;
  for (WatchVector::iterator watch_entry(watches_.begin());
       watch_entry != watches_.end(); ++watch_entry) {
    InotifyReader::Watch old_watch = watch_entry->watch_;
    if (path_valid) {
      watch_entry->watch_ = g_inotify_reader.Get().AddWatch(path, this);
      if (watch_entry->watch_ == InotifyReader::kInvalidWatch) {
        path_valid = false;
      }
    } else {
      watch_entry->watch_ = InotifyReader::kInvalidWatch;
    }
    if (old_watch != InotifyReader::kInvalidWatch &&
        old_watch != watch_entry->watch_) {
      g_inotify_reader.Get().RemoveWatch(old_watch, this);
    }
    path = path.Append(watch_entry->subdir_);
  }

  return true;
}

}  // namespace

FilePathWatcher::FilePathWatcher() {
  impl_ = new FilePathWatcherImpl();
}

}  // namespace files
}  // namespace base
