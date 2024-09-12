// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BASE_MESSAGE_PUMP_IO_STARBOARD_H_
#define BASE_MESSAGE_PUMP_IO_STARBOARD_H_

#include "starboard/configuration.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "starboard/common/socket.h"
#include "starboard/socket_waiter.h"

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O.
class BASE_EXPORT MessagePumpIOStarboard : public MessagePump {
 public:
  class IOObserver : public CheckedObserver {
   public:
    IOObserver() {}
    virtual ~IOObserver() {}

    // An IOObserver is an object that receives IO notifications from the
    // MessagePump.
    //
    // NOTE: An IOObserver should not do much work, it should return extremely
    // quickly!
    virtual void WillProcessIOEvent() = 0;
    virtual void DidProcessIOEvent() = 0;
  };

  // Used with WatchFileDescriptor to asynchronously monitor the I/O readiness
  // of a file descriptor.
  class Watcher {
   public:
    // These methods are called from MessageLoop::Run when a socket can be
    // interacted with without blocking.
#if SB_API_VERSION >= 16
    virtual void OnFileCanReadWithoutBlocking(int socket) {}
    virtual void OnFileCanWriteWithoutBlocking(int socket) {}
#else
    virtual void OnSocketReadyToRead(SbSocket socket) {}
    virtual void OnSocketReadyToWrite(SbSocket socket) {}
#endif

   protected:
    virtual ~Watcher() {}
  };

  // Object returned by WatchSocket to manage further watching.
  class SocketWatcher {
   public:
    SocketWatcher(const Location& from_here);
    ~SocketWatcher();  // Implicitly calls StopWatchingSocket.

    SocketWatcher(const SocketWatcher&) = delete;
    SocketWatcher& operator=(const SocketWatcher&) = delete;

    // NOTE: These methods aren't called StartWatching()/StopWatching() to avoid
    // confusion with the win32 ObjectWatcher class.

    // Stops watching the socket, always safe to call.  No-op if there's nothing
    // to do.
#if SB_API_VERSION >= 16
    bool StopWatchingFileDescriptor();
#else
    bool StopWatchingSocket();
#endif

    bool persistent() const { return persistent_; }

   private:
    friend class MessagePumpIOStarboard;
    friend class MessagePumpIOStarboardTest;

    // Called by MessagePumpIOStarboard.
#if SB_API_VERSION >= 16
    void Init(int socket, bool persistent);
    int Release();
#else
    void Init(SbSocket socket, bool persistent);
    SbSocket Release();
#endif

    int interests() const { return interests_; }
    void set_interests(int interests) { interests_ = interests; }

    void set_pump(MessagePumpIOStarboard* pump) { pump_ = pump; }
    MessagePumpIOStarboard* pump() const { return pump_; }

    void set_watcher(Watcher* watcher) { watcher_ = watcher; }

#if SB_API_VERSION >= 16
    void OnFileCanReadWithoutBlocking(int socket, MessagePumpIOStarboard* pump);
    void OnFileCanWriteWithoutBlocking(int socket, MessagePumpIOStarboard* pump);
#else
    void OnSocketReadyToRead(SbSocket socket, MessagePumpIOStarboard* pump);
    void OnSocketReadyToWrite(SbSocket socket, MessagePumpIOStarboard* pump);
#endif

    const Location created_from_location_;
    int interests_;
#if SB_API_VERSION >= 16
    int socket_;
#else
    SbSocket socket_;
#endif
    bool persistent_;
    MessagePumpIOStarboard* pump_;
    Watcher* watcher_;
    base::WeakPtrFactory<SocketWatcher> weak_factory_;
  };

  enum Mode {
    WATCH_READ = 1 << 0,
    WATCH_WRITE = 1 << 1,
    WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
  };

  MessagePumpIOStarboard();
  virtual ~MessagePumpIOStarboard();

  MessagePumpIOStarboard(const MessagePumpIOStarboard&) = delete;
  MessagePumpIOStarboard& operator=(const MessagePumpIOStarboard&) = delete;

  // Have the current thread's message loop watch for a a situation in which
  // reading/writing to the socket can be performed without blocking.  Callers
  // must provide a preallocated SocketWatcher object which can later be used to
  // manage the lifetime of this event.  If a SocketWatcher is passed in which
  // is already attached to a socket, then the effect is cumulative i.e. after
  // the call |controller| will watch both the previous event and the new one.
  // If an error occurs while calling this method in a cumulative fashion, the
  // event previously attached to |controller| is aborted.  Returns true on
  // success.  Must be called on the same thread the message_pump is running on.
#if SB_API_VERSION >= 16
  bool WatchFileDescriptor(int socket,
                           bool persistent,
                           int mode,
                           SocketWatcher* controller,
                           Watcher* delegate);

  // Stops watching the socket.
  bool StopWatchingFileDescriptor(int socket);
#else
  bool Watch(SbSocket socket,
             bool persistent,
             int mode,
             SocketWatcher* controller,
             Watcher* delegate);

  // Stops watching the socket.
  bool StopWatching(SbSocket socket);
#endif

  void AddIOObserver(IOObserver* obs);
  void RemoveIOObserver(IOObserver* obs);

  // MessagePump methods:
  virtual void Run(Delegate* delegate) override;
  virtual void Quit() override;
  virtual void ScheduleWork() override;
  virtual void ScheduleDelayedWork(const Delegate::NextWorkInfo& next_work_info) override;

 private:
  friend class MessagePumpIOStarboardTest;

  void WillProcessIOEvent();
  void DidProcessIOEvent();

  // Called by SbSocketWaiter to tell us a registered socket can be read and/or
  // written to.
#if SB_API_VERSION >= 16
  static void OnPosixSocketWaiterNotification(SbSocketWaiter waiter,
                                              int socket,
                                              void* context,
                                              int ready_interests);
#else
  static void OnSocketWaiterNotification(SbSocketWaiter waiter,
                                         SbSocket socket,
                                         void* context,
                                         int ready_interests);
#endif

  bool should_quit() const { return !keep_running_; }

  // This flag is set to false when Run should return.
  bool keep_running_;

  // This flag is set if the Socket Waiter has processed I/O events.
  bool processed_io_events_;

  // Starboard socket waiter dispatcher.  Waits for all sockets registered with
  // it, and sends readiness callbacks when a socket is ready for I/O.
  SbSocketWaiter waiter_;

  ObserverList<IOObserver> io_observers_;
  THREAD_CHECKER(watch_socket_caller_checker_);
};

using MessagePumpForIO = MessagePumpIOStarboard;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_IO_STARBOARD_H_
