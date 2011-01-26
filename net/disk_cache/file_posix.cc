// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include <fcntl.h>

#include "base/logging.h"
#include "base/threading/worker_pool.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/in_flight_io.h"

namespace {

// This class represents a single asynchronous IO operation while it is being
// bounced between threads.
class FileBackgroundIO : public disk_cache::BackgroundIO {
 public:
  // Other than the actual parameters for the IO operation (including the
  // |callback| that must be notified at the end), we need the controller that
  // is keeping track of all operations. When done, we notify the controller
  // (we do NOT invoke the callback), in the worker thead that completed the
  // operation.
  FileBackgroundIO(disk_cache::File* file, const void* buf, size_t buf_len,
                   size_t offset, disk_cache::FileIOCallback* callback,
                   disk_cache::InFlightIO* controller)
      : disk_cache::BackgroundIO(controller), callback_(callback), file_(file),
        buf_(buf), buf_len_(buf_len), offset_(offset) {
  }

  disk_cache::FileIOCallback* callback() {
    return callback_;
  }

  disk_cache::File* file() {
    return file_;
  }

  // Read and Write are the operations that can be performed asynchronously.
  // The actual parameters for the operation are setup in the constructor of
  // the object. Both methods should be called from a worker thread, by posting
  // a task to the WorkerPool (they are RunnableMethods). When finished,
  // controller->OnIOComplete() is called.
  void Read();
  void Write();

 private:
  ~FileBackgroundIO() {}

  disk_cache::FileIOCallback* callback_;

  disk_cache::File* file_;
  const void* buf_;
  size_t buf_len_;
  size_t offset_;

  DISALLOW_COPY_AND_ASSIGN(FileBackgroundIO);
};


// The specialized controller that keeps track of current operations.
class FileInFlightIO : public disk_cache::InFlightIO {
 public:
  FileInFlightIO() {}
  ~FileInFlightIO() {}

  // These methods start an asynchronous operation. The arguments have the same
  // semantics of the File asynchronous operations, with the exception that the
  // operation never finishes synchronously.
  void PostRead(disk_cache::File* file, void* buf, size_t buf_len,
                size_t offset, disk_cache::FileIOCallback* callback);
  void PostWrite(disk_cache::File* file, const void* buf, size_t buf_len,
                 size_t offset, disk_cache::FileIOCallback* callback);

 protected:
  // Invokes the users' completion callback at the end of the IO operation.
  // |cancel| is true if the actual task posted to the thread is still
  // queued (because we are inside WaitForPendingIO), and false if said task is
  // the one performing the call.
  virtual void OnOperationComplete(disk_cache::BackgroundIO* operation,
                                   bool cancel);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileInFlightIO);
};

// ---------------------------------------------------------------------------

// Runs on a worker thread.
void FileBackgroundIO::Read() {
  if (file_->Read(const_cast<void*>(buf_), buf_len_, offset_)) {
    result_ = static_cast<int>(buf_len_);
  } else {
    result_ = -1;
  }
  controller_->OnIOComplete(this);
}

// Runs on a worker thread.
void FileBackgroundIO::Write() {
  bool rv = file_->Write(buf_, buf_len_, offset_);

  result_ = rv ? static_cast<int>(buf_len_) : -1;
  controller_->OnIOComplete(this);
}

// ---------------------------------------------------------------------------

void FileInFlightIO::PostRead(disk_cache::File *file, void* buf, size_t buf_len,
                          size_t offset, disk_cache::FileIOCallback *callback) {
  scoped_refptr<FileBackgroundIO> operation(
      new FileBackgroundIO(file, buf, buf_len, offset, callback, this));
  file->AddRef();  // Balanced on OnOperationComplete()

  base::WorkerPool::PostTask(FROM_HERE,
      NewRunnableMethod(operation.get(), &FileBackgroundIO::Read), true);
  OnOperationPosted(operation);
}

void FileInFlightIO::PostWrite(disk_cache::File* file, const void* buf,
                           size_t buf_len, size_t offset,
                           disk_cache::FileIOCallback* callback) {
  scoped_refptr<FileBackgroundIO> operation(
      new FileBackgroundIO(file, buf, buf_len, offset, callback, this));
  file->AddRef();  // Balanced on OnOperationComplete()

  base::WorkerPool::PostTask(FROM_HERE,
      NewRunnableMethod(operation.get(), &FileBackgroundIO::Write), true);
  OnOperationPosted(operation);
}

// Runs on the IO thread.
void FileInFlightIO::OnOperationComplete(disk_cache::BackgroundIO* operation,
                                         bool cancel) {
  FileBackgroundIO* op = static_cast<FileBackgroundIO*>(operation);

  disk_cache::FileIOCallback* callback = op->callback();
  int bytes = operation->result();

  // Release the references acquired in PostRead / PostWrite.
  op->file()->Release();
  callback->OnFileIOComplete(bytes);
}

// A static object tha will broker all async operations.
FileInFlightIO* s_file_operations = NULL;

// Returns the current FileInFlightIO.
FileInFlightIO* GetFileInFlightIO() {
  if (!s_file_operations) {
    s_file_operations = new FileInFlightIO;
  }
  return s_file_operations;
}

// Deletes the current FileInFlightIO.
void DeleteFileInFlightIO() {
  DCHECK(s_file_operations);
  delete s_file_operations;
  s_file_operations = NULL;
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

  GetFileInFlightIO()->PostRead(this, buffer, buffer_len, offset, callback);

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
  // We may be running unit tests so we should allow be able to reset the
  // message loop.
  GetFileInFlightIO()->WaitForPendingIO();
  DeleteFileInFlightIO();
}

File::~File() {
  if (platform_file_)
    close(platform_file_);
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  GetFileInFlightIO()->PostWrite(this, buffer, buffer_len, offset, callback);

  if (completed)
    *completed = false;
  return true;
}

}  // namespace disk_cache
