// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CROSS_PROCESS_NOTIFICATION_H_
#define MEDIA_AUDIO_CROSS_PROCESS_NOTIFICATION_H_

#include <vector>

#include "base/basictypes.h"
#include "base/process.h"
#include "base/threading/non_thread_safe.h"
#include "media/base/media_export.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#else
#include "base/file_descriptor_posix.h"
#include "base/sync_socket.h"
#endif

// A mechanism to synchronize access to a shared resource between two parties
// when the usage pattern resembles that of two players playing a game of chess.
// Each end has an instance of CrossProcessNotification and calls Signal() when
// it has finished using the shared resource.
// Before accessing the resource, it must call Wait() in order to know when the
// other end has called Signal().
//
// Here's some pseudo code for how this class can be used:
//
// This method is used by both processes as it's a general way to use the
// shared resource and then grant the privilege to the other process:
//
// void WriteToSharedMemory(CrossProcessNotification* notification,
//                          SharedMemory* mem,
//                          const char my_char) {
//   notification->Wait();  // Wait for the other process to yield access.
//   reinterpret_cast<char*>(mem->memory())[0] = my_char;
//   notification->Signal();  // Grant the other process access.
// }
//
// Process A:
//
// class A {
//  public:
//   void Initialize(base::ProcessHandle process_b) {
//     mem_.CreateNamed("foo", false, 1024);
//
//     CrossProcessNotification other;
//     CHECK(CrossProcessNotification::InitializePair(&notification_, &other));
//     CrossProcessNotification::IPCHandle handle_1, handle_2;
//     CHECK(other.ShareToProcess(process_b, &handle_1, &handle_2));
//     // This could be implemented by using some IPC mechanism
//     // such as MessageLoop.
//     SendToProcessB(mem_, handle_1, handle_2);
//     // Allow process B the first chance to write to the memory:
//     notification_.Signal();
//     // Once B is done, we'll write 'A' to the shared memory.
//     WriteToSharedMemory(&notification_, &mem_, 'A');
//   }
//
//   CrossProcessNotification notification_;
//   SharedMemory mem_;
// };
//
// Process B:
//
// class B {
//  public:
//   // Called when we receive the IPC message from A.
//   void Initialize(SharedMemoryHandle mem,
//                   CrossProcessNotification::IPCHandle handle_1,
//                   CrossProcessNotification::IPCHandle handle_2) {
//     mem_.reset(new SharedMemory(mem, false));
//     notification_.reset(new CrossProcessNotification(handle_1, handle_2));
//     WriteToSharedMemory(&notification_, &mem_, 'B');
//   }
//
//   CrossProcessNotification notification_;
//   scoped_ptr<SharedMemory> mem_;
// };
//
class MEDIA_EXPORT CrossProcessNotification {
 public:
#if defined(OS_WIN)
  typedef HANDLE IPCHandle;
#else
  typedef base::FileDescriptor IPCHandle;
#endif

  typedef std::vector<CrossProcessNotification*> Notifications;

  // Default ctor.  Initializes a NULL notification.  User must call
  // InitializePair() to initialize the instance along with a connected one.
  CrossProcessNotification();

  // Ctor for the user that does not call InitializePair but instead receives
  // handles from the one that did.  These handles come from a call to
  // ShareToProcess.
  CrossProcessNotification(IPCHandle handle_1, IPCHandle handle_2);
  ~CrossProcessNotification();

  // Raises a signal that the shared resource now can be accessed by the other
  // party.
  // NOTE: Calling Signal() more than once without calling Wait() in between
  // is not a supported scenario and will result in undefined behavior (and
  // different depending on platform).
  void Signal();

  // Waits for the other party to finish using the shared resource.
  // NOTE: As with Signal(), you must not call Wait() more than once without
  // calling Signal() in between.
  void Wait();

  bool IsValid() const;

  // Copies the internal handles to the output parameters, |handle_1| and
  // |handle_2|.  The operation can fail, so the caller must be prepared to
  // handle that case.
  bool ShareToProcess(base::ProcessHandle process, IPCHandle* handle_1,
                      IPCHandle* handle_2);

  // Initializes a pair of CrossProcessNotification instances.  Note that this
  // can fail (e.g. due to EMFILE on Linux).
  static bool InitializePair(CrossProcessNotification* a,
                             CrossProcessNotification* b);

  // Use an instance of this class when you have to repeatedly wait for multiple
  // notifications on the same thread.  The class will store information about
  // which notification was last signaled and try to distribute the signals so
  // that all notifications get a chance to be processed in times of high load
  // and a busy one won't starve the others.
  // TODO(tommi): Support a way to abort the wait.
  class MEDIA_EXPORT WaitForMultiple :
      public NON_EXPORTED_BASE(base::NonThreadSafe) {
   public:
    // Caller must make sure that the lifetime of the array is greater than
    // that of the WaitForMultiple instance.
    explicit WaitForMultiple(const Notifications* notifications);

    // Waits for any of the notifications to be signaled.  Returns the 0 based
    // index of a signaled notification.
    int Wait();

    // Call when the array changes.  This should be called on the same thread
    // as Wait() is called on and the array must never change while a Wait()
    // is in progress.
    void Reset(const Notifications* notifications);

   private:
    const Notifications* notifications_;
    size_t wait_offset_;
  };

 private:
  // Only called by the WaitForMultiple class.  See documentation
  // for WaitForMultiple and comments inside WaitMultiple for details.
  static int WaitMultiple(const Notifications& notifications,
                          size_t wait_offset);

#if defined(OS_WIN)
  base::win::ScopedHandle mine_;
  base::win::ScopedHandle other_;
#else
  typedef base::CancelableSyncSocket SocketClass;
  SocketClass socket_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrossProcessNotification);
};

#endif  // MEDIA_AUDIO_CROSS_PROCESS_NOTIFICATION_H_
