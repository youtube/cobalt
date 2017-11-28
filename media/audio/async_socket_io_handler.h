// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ASYNC_SOCKET_IO_HANDLER_H_
#define MEDIA_AUDIO_ASYNC_SOCKET_IO_HANDLER_H_

#include "base/message_loop.h"
#include "base/sync_socket.h"
#include "base/threading/non_thread_safe.h"
#include "media/base/media_export.h"

namespace media {

// The message loop callback interface is different based on platforms.
#if defined(OS_WIN)
typedef MessageLoopForIO::IOHandler MessageLoopIOHandler;
#elif defined(OS_POSIX)
typedef MessageLoopForIO::Watcher MessageLoopIOHandler;
#endif

// Extends the CancelableSyncSocket class to allow reading from a socket
// asynchronously on a TYPE_IO message loop thread.  This makes it easy to share
// a thread that uses a message loop (e.g. for IPC and other things) and not
// require a separate thread to read from the socket.
//
// Example usage (also see the unit tests):
//
// class SocketReader {
//  public:
//   SocketReader(base::CancelableSyncSocket* socket)
//       : socket_(socket), buffer_() {
//     io_handler.Initialize(socket_->handle(),
//                           base::Bind(&SocketReader::OnDataAvailable,
//                                      base::Unretained(this));
//   }
//
//   void AsyncRead() {
//     CHECK(io_handler.Read(&buffer_[0], sizeof(buffer_)));
//   }
//
//  private:
//   void OnDataAvailable(int bytes_read) {
//     if (ProcessData(&buffer_[0], bytes_read)) {
//       // Issue another read.
//       CHECK(io_handler.Read(&buffer_[0], sizeof(buffer_)));
//     }
//   }
//
//   media::AsyncSocketIoHandler io_handler;
//   base::CancelableSyncSocket* socket_;
//   char buffer_[kBufferSize];
// };
//
class MEDIA_EXPORT AsyncSocketIoHandler
    : public NON_EXPORTED_BASE(base::NonThreadSafe),
      public NON_EXPORTED_BASE(MessageLoopIOHandler) {
 public:
  AsyncSocketIoHandler();
  virtual ~AsyncSocketIoHandler();

  // Type definition for the callback. The parameter tells how many
  // bytes were read and is 0 if an error occurred.
  typedef base::Callback<void(int)> ReadCompleteCallback;

  // Initializes the AsyncSocketIoHandler by hooking it up to the current
  // thread's message loop (must be TYPE_IO), to do async reads from the socket
  // on the current thread.  The |callback| will be invoked whenever a Read()
  // has completed.
  bool Initialize(base::SyncSocket::Handle socket,
                  const ReadCompleteCallback& callback);

  // Attempts to read from the socket.  The return value will be |false|
  // if an error occurred and |true| if data was read or a pending read
  // was issued.  Regardless of async or sync operation, the
  // ReadCompleteCallback (see above) will be called when data is available.
  bool Read(char* buffer, int buffer_len);

 private:
#if defined(OS_WIN)
  // Implementation of IOHandler on Windows.
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error) override;
#elif defined(OS_POSIX)
  // Implementation of MessageLoopForIO::Watcher.
  virtual void OnFileCanWriteWithoutBlocking(int socket) override {}
  virtual void OnFileCanReadWithoutBlocking(int socket) override;

  void EnsureWatchingSocket();
#endif

  base::SyncSocket::Handle socket_;
#if defined(OS_WIN)
  MessageLoopForIO::IOContext* context_;
  bool is_pending_;
#elif defined(OS_POSIX)
  MessageLoopForIO::FileDescriptorWatcher socket_watcher_;
  // |pending_buffer_| and |pending_buffer_len_| are valid only between
  // Read() and OnFileCanReadWithoutBlocking().
  char* pending_buffer_;
  int pending_buffer_len_;
  // |true| iff the message loop is watching the socket for IO events.
  bool is_watching_;
#endif
  ReadCompleteCallback read_complete_;

  DISALLOW_COPY_AND_ASSIGN(AsyncSocketIoHandler);
};

}  // namespace media.

#endif  // MEDIA_AUDIO_ASYNC_SOCKET_IO_HANDLER_H_
