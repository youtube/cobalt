// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include <fcntl.h>

#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/waitable_event.h"
#include "base/worker_pool.h"
#include "net/disk_cache/disk_cache.h"

namespace {

class InFlightIO;

// This class represents a single asynchronous IO operation while it is being
// bounced between threads.
class BackgroundIO : public base::RefCountedThreadSafe<BackgroundIO> {
 public:
  // Other than the actual parameters for the IO operation (including the
  // |callback| that must be notified at the end), we need the controller that
  // is keeping track of all operations. When done, we notify the controller
  // (we do NOT invoke the callback), in the worker thead that completed the
  // operation.
  BackgroundIO(disk_cache::File* file, const void* buf, size_t buf_len,
               size_t offset, disk_cache::FileIOCallback* callback,
               InFlightIO* controller)
      : io_completed_(true, false), callback_(callback), file_(file), buf_(buf),
        buf_len_(buf_len), offset_(offset), controller_(controller),
        bytes_(0) {}

  // Read and Write are the operations that can be performed asynchronously.
  // The actual parameters for the operation are setup in the constructor of
  // the object. Both methods should be called from a worker thread, by posting
  // a task to the WorkerPool (they are RunnableMethods). When finished,
  // controller->OnIOComplete() is called.
  void Read();
  void Write();

  // This method signals the controller that this operation is finished, in the
  // original thread (presumably the IO-Thread). In practice, this is a
  // RunableMethod that allows cancellation.
  void OnIOSignalled();

  // Allows the cancellation of the task to notify the controller (step number 7
  // in the diagram below). In practice, if the controller waits for the
  // operation to finish it doesn't have to wait for the final task to be
  // processed by the message loop so calling this method prevents its delivery.
  // Note that this method is not intended to cancel the actual IO operation or
  // to prevent the first notification to take place (OnIOComplete).
  void Cancel();

  // Retrieves the number of bytes transfered.
  int Result();

  base::WaitableEvent* io_completed() {
    return &io_completed_;
  }

  disk_cache::FileIOCallback* callback() {
    return callback_;
  }

  disk_cache::File* file() {
    return file_;
  }

 private:
  friend class base::RefCountedThreadSafe<BackgroundIO>;
  ~BackgroundIO() {}

  // An event to signal when the operation completes, and the user callback that
  // has to be invoked. These members are accessed directly by the controller.
  base::WaitableEvent io_completed_;
  disk_cache::FileIOCallback* callback_;

  disk_cache::File* file_;
  const void* buf_;
  size_t buf_len_;
  size_t offset_;
  InFlightIO* controller_;  // The controller that tracks all operations.
  int bytes_;  // Final operation result.

  DISALLOW_COPY_AND_ASSIGN(BackgroundIO);
};

// This class keeps track of every asynchronous IO operation. A single instance
// of this class is meant to be used to start an asynchronous operation (using
// PostRead/PostWrite). This class will post the operation to a worker thread,
// hanlde the notification when the operation finishes and perform the callback
// on the same thread that was used to start the operation.
//
// The regular sequence of calls is:
//                 Thread_1                          Worker_thread
//    1.     InFlightIO::PostRead()
//    2.                         -> PostTask ->
//    3.                                          BackgroundIO::Read()
//    4.                                         IO operation completes
//    5.                                       InFlightIO::OnIOComplete()
//    6.                         <- PostTask <-
//    7.  BackgroundIO::OnIOSignalled()
//    8.  InFlightIO::InvokeCallback()
//    9.       invoke callback
//
// Shutdown is a special case that is handled though WaitForPendingIO() instead
// of just waiting for step 7.
class InFlightIO {
 public:
  InFlightIO() : callback_thread_(MessageLoop::current()) {}
  ~InFlightIO() {}

  // These methods start an asynchronous operation. The arguments have the same
  // semantics of the File asynchronous operations, with the exception that the
  // operation never finishes synchronously.
  void PostRead(disk_cache::File* file, void* buf, size_t buf_len,
                size_t offset, disk_cache::FileIOCallback* callback);
  void PostWrite(disk_cache::File* file, const void* buf, size_t buf_len,
                 size_t offset, disk_cache::FileIOCallback* callback);

  // Blocks the current thread until all IO operations tracked by this object
  // complete.
  void WaitForPendingIO();

  // Called on a worker thread when |operation| completes.
  void OnIOComplete(BackgroundIO* operation);

  // Invokes the users' completion callback at the end of the IO operation.
  // |cancel_task| is true if the actual task posted to the thread is still
  // queued (because we are inside WaitForPendingIO), and false if said task is
  // the one performing the call.
  void InvokeCallback(BackgroundIO* operation, bool cancel_task);

 private:
  typedef std::set<scoped_refptr<BackgroundIO> > IOList;

  IOList io_list_;  // List of pending io operations.
  MessageLoop* callback_thread_;
};

// ---------------------------------------------------------------------------

// Runs on a worker thread.
void BackgroundIO::Read() {
  if (file_->Read(const_cast<void*>(buf_), buf_len_, offset_)) {
    bytes_ = static_cast<int>(buf_len_);
  } else {
    bytes_ = -1;
  }
  controller_->OnIOComplete(this);
}

int BackgroundIO::Result() {
  return bytes_;
}

void BackgroundIO::Cancel() {
  DCHECK(controller_);
  controller_ = NULL;
}

// Runs on a worker thread.
void BackgroundIO::Write() {
  bool rv = file_->Write(buf_, buf_len_, offset_);

  bytes_ = rv ? static_cast<int>(buf_len_) : -1;
  controller_->OnIOComplete(this);
}

// Runs on the IO thread.
void BackgroundIO::OnIOSignalled() {
  if (controller_)
    controller_->InvokeCallback(this, false);
}

// ---------------------------------------------------------------------------

void InFlightIO::PostRead(disk_cache::File *file, void* buf, size_t buf_len,
                          size_t offset, disk_cache::FileIOCallback *callback) {
  scoped_refptr<BackgroundIO> operation =
      new BackgroundIO(file, buf, buf_len, offset, callback, this);
  io_list_.insert(operation.get());
  file->AddRef();  // Balanced on InvokeCallback()

  if (!callback_thread_)
      callback_thread_ = MessageLoop::current();

  WorkerPool::PostTask(FROM_HERE,
                       NewRunnableMethod(operation.get(), &BackgroundIO::Read),
                       true);
}

void InFlightIO::PostWrite(disk_cache::File* file, const void* buf,
                           size_t buf_len, size_t offset,
                           disk_cache::FileIOCallback* callback) {
  scoped_refptr<BackgroundIO> operation =
      new BackgroundIO(file, buf, buf_len, offset, callback, this);
  io_list_.insert(operation.get());
  file->AddRef();  // Balanced on InvokeCallback()

  if (!callback_thread_)
    callback_thread_ = MessageLoop::current();

  WorkerPool::PostTask(FROM_HERE,
                       NewRunnableMethod(operation.get(), &BackgroundIO::Write),
                       true);
}

void InFlightIO::WaitForPendingIO() {
  while (!io_list_.empty()) {
    // Block the current thread until all pending IO completes.
    IOList::iterator it = io_list_.begin();
    InvokeCallback(*it, true);
  }
  // Unit tests can use different threads.
  callback_thread_ = NULL;
}

// Runs on a worker thread.
void InFlightIO::OnIOComplete(BackgroundIO* operation) {
  callback_thread_->PostTask(FROM_HERE,
                             NewRunnableMethod(operation,
                                               &BackgroundIO::OnIOSignalled));
  operation->io_completed()->Signal();
}

// Runs on the IO thread.
void InFlightIO::InvokeCallback(BackgroundIO* operation, bool cancel_task) {
  operation->io_completed()->Wait();

  if (cancel_task)
    operation->Cancel();

  disk_cache::FileIOCallback* callback = operation->callback();
  int bytes = operation->Result();

  // Release the references acquired in PostRead / PostWrite.
  operation->file()->Release();
  io_list_.erase(operation);
  callback->OnFileIOComplete(bytes);
}

}  // namespace

namespace disk_cache {

File::File(base::PlatformFile file)
    : init_(true),
      mixed_(true),
      platform_file_(file),
      sync_platform_file_(base::kInvalidPlatformFileValue) {
}

bool File::Init(const FilePath& name) {
  if (init_)
    return false;

  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;
  platform_file_ = base::CreatePlatformFile(name, flags, NULL, NULL);
  if (platform_file_ < 0) {
    platform_file_ = 0;
    return false;
  }

  init_ = true;
  return true;
}

File::~File() {
  if (platform_file_)
    close(platform_file_);
}

base::PlatformFile File::platform_file() const {
  return platform_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (base::kInvalidPlatformFileValue != platform_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  int ret = pread(platform_file_, buffer, buffer_len, offset);
  return (static_cast<size_t>(ret) == buffer_len);
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  int ret = pwrite(platform_file_, buffer, buffer_len, offset);
  return (static_cast<size_t>(ret) == buffer_len);
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Read(buffer, buffer_len, offset);
  }

  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  InFlightIO* io_operations = Singleton<InFlightIO>::get();
  io_operations->PostRead(this, buffer, buffer_len, offset, callback);

  *completed = false;
  return true;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Write(buffer, buffer_len, offset);
  }

  return AsyncWrite(buffer, buffer_len, offset, callback, completed);
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  InFlightIO* io_operations = Singleton<InFlightIO>::get();
  io_operations->PostWrite(this, buffer, buffer_len, offset, callback);

  if (completed)
    *completed = false;
  return true;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  return 0 == ftruncate(platform_file_, length);
}

size_t File::GetLength() {
  DCHECK(init_);
  size_t ret = lseek(platform_file_, 0, SEEK_END);
  return ret;
}

// Static.
void File::WaitForPendingIO(int* num_pending_io) {
  // We may be running unit tests so we should allow InFlightIO to reset the
  // message loop.
  Singleton<InFlightIO>::get()->WaitForPendingIO();
}

}  // namespace disk_cache
