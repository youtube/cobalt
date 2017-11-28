// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream::Context class.
// The general design of FileStream is as follows: file_stream.h defines
// FileStream class which basically is just an "wrapper" not containing any
// specific implementation details. It re-routes all its method calls to
// the instance of FileStream::Context (FileStream holds a scoped_ptr to
// FileStream::Context instance). Context was extracted into a different class
// to be able to do and finish all async operations even when FileStream
// instance is deleted. So FileStream's destructor can schedule file
// closing to be done by Context in WorkerPool and then just return (releasing
// Context pointer from scoped_ptr) without waiting for actual closing to
// complete.
// Implementation of FileStream::Context is divided in two parts: some methods
// and members are platform-independent and some depend on the platform. This
// header file contains the complete definition of Context class including all
// platform-dependent parts (because of that it has a lot of #if-#else
// branching). Implementations of all platform-independent methods are
// located in file_stream_context.cc, and all platform-dependent methods are
// in file_stream_context_{win,posix}.cc. This separation provides better
// readability of Context's code. And we tried to make as much Context code
// platform-independent as possible. So file_stream_context_{win,posix}.cc are
// much smaller than file_stream_context.cc now.

#ifndef NET_BASE_FILE_STREAM_CONTEXT_H_
#define NET_BASE_FILE_STREAM_CONTEXT_H_

#include "base/message_loop.h"
#include "base/platform_file.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "net/base/file_stream_metrics.h"
#include "net/base/file_stream_whence.h"
#include "net/base/net_log.h"

#if defined(OS_POSIX)
#include <errno.h>
#elif defined(OS_STARBOARD)
#include "starboard/system.h"
#endif

class FilePath;

namespace net {

class IOBuffer;

#if defined(OS_WIN)
class FileStream::Context : public MessageLoopForIO::IOHandler {
#elif defined(OS_POSIX) || defined(OS_STARBOARD)
class FileStream::Context {
#endif
 public:
  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  explicit Context(const BoundNetLog& bound_net_log);
  Context(base::PlatformFile file,
          const BoundNetLog& bound_net_log,
          int open_flags);
#if defined(OS_WIN)
  virtual ~Context();
#elif defined(OS_POSIX) || defined(OS_STARBOARD)
  ~Context();
#endif

  int64 GetFileSize() const;

  int ReadAsync(IOBuffer* buf,
                int buf_len,
                const CompletionCallback& callback);
  int ReadSync(char* buf, int buf_len);

  int WriteAsync(IOBuffer* buf,
                 int buf_len,
                 const CompletionCallback& callback);
  int WriteSync(const char* buf, int buf_len);

  int Truncate(int64 bytes);

  ////////////////////////////////////////////////////////////////////////////
  // Inline methods.
  ////////////////////////////////////////////////////////////////////////////

  void set_record_uma(bool value) { record_uma_ = value; }
  base::PlatformFile file() const { return file_; }
  bool async_in_progress() const { return async_in_progress_; }

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Destroys the context. It can be deleted in the method or deletion can be
  // deferred if some asynchronous operation is now in progress or if file is
  // not closed yet.
  void Orphan();

  void OpenAsync(const FilePath& path,
                 int open_flags,
                 const CompletionCallback& callback);
  int OpenSync(const FilePath& path, int open_flags);

  void CloseSync();

  void SeekAsync(Whence whence,
                 int64 offset,
                 const Int64CompletionCallback& callback);
  int64 SeekSync(Whence whence, int64 offset);

  void FlushAsync(const CompletionCallback& callback);
  int FlushSync();

 private:
  ////////////////////////////////////////////////////////////////////////////
  // Error code that is platform-dependent but is used in the platform-
  // independent code implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////
  enum {
#if defined(OS_WIN)
    ERROR_BAD_FILE = ERROR_INVALID_HANDLE
#elif defined(OS_POSIX)
    ERROR_BAD_FILE = EBADF
#elif defined(OS_STARBOARD)
    ERROR_BAD_FILE = kSbFileErrorFailed
#endif
  };

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  struct OpenResult {
    base::PlatformFile file;
    int error_code;
  };

  // Map system error into network error code and log it with |bound_net_log_|.
  int RecordAndMapError(int error, FileErrorSource source) const;

  void BeginOpenEvent(const FilePath& path);

  OpenResult OpenFileImpl(const FilePath& path, int open_flags);

  int ProcessOpenError(int error_code);
  void OnOpenCompleted(const CompletionCallback& callback, OpenResult result);

  void CloseAndDelete();
  void OnCloseCompleted();

  Int64CompletionCallback IntToInt64(const CompletionCallback& callback);

  // Checks for IO error that probably happened in async methods.
  // If there was error reports it.
  void CheckForIOError(int64* result, FileErrorSource source);

  // Called when asynchronous Seek() is completed.
  // Reports error if needed and calls callback.
  void ProcessAsyncResult(const Int64CompletionCallback& callback,
                          FileErrorSource source,
                          int64 result);

  // Called when asynchronous Open() or Seek()
  // is completed. |result| contains the result or a network error code.
  void OnAsyncCompleted(const Int64CompletionCallback& callback, int64 result);

  ////////////////////////////////////////////////////////////////////////////
  // Helper stuff which is platform-dependent but is used in the platform-
  // independent code implemented in file_stream_context.cc. These helpers were
  // introduced solely to implement as much of the Context methods as
  // possible independently from platform.
  ////////////////////////////////////////////////////////////////////////////

#if defined(OS_WIN)
  int GetLastErrno() { return GetLastError(); }
  void OnAsyncFileOpened();
#elif defined(OS_POSIX)
  int GetLastErrno() { return errno; }
  void OnAsyncFileOpened() {}
  void CancelIo(base::PlatformFile) {}
#elif defined(OS_STARBOARD)
  int GetLastErrno() const {
    return SbSystemGetLastError();
  }
  void OnAsyncFileOpened() {}
  void CancelIo(base::PlatformFile) {}
#endif

  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Adjusts the position from where the data is read.
  int64 SeekFileImpl(Whence whence, int64 offset);

  // Flushes all data written to the stream.
  int64 FlushFileImpl();

#if defined(OS_WIN)
  void IOCompletionIsPending(const CompletionCallback& callback, IOBuffer* buf);

  // Implementation of MessageLoopForIO::IOHandler
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_read,
                             DWORD error) override;
#elif defined(OS_POSIX) || defined(OS_STARBOARD)
  // ReadFileImpl() is a simple wrapper around read() that handles EINTR
  // signals and calls RecordAndMapError() to map errno to net error codes.
  int64 ReadFileImpl(scoped_refptr<IOBuffer> buf, int buf_len);

  // WriteFileImpl() is a simple wrapper around write() that handles EINTR
  // signals and calls MapSystemError() to map errno to net error codes.
  // It tries to write to completion.
  int64 WriteFileImpl(scoped_refptr<IOBuffer> buf, int buf_len);
#endif

  base::PlatformFile file_;
  bool record_uma_;
  bool async_in_progress_;
  bool orphaned_;
  BoundNetLog bound_net_log_;

#if defined(OS_WIN)
  MessageLoopForIO::IOContext io_context_;
  CompletionCallback callback_;
  scoped_refptr<IOBuffer> in_flight_buf_;
  FileErrorSource error_source_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_CONTEXT_H_
