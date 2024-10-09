// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_SOCKET_WAITER_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_SOCKET_WAITER_INTERNAL_H_

#include <windows.h>
#include <winsock2.h>

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/socket.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/win32/auto_event_handle.h"
#include "starboard/shared/win32/socket_internal.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace sbwin32 = starboard::shared::win32;

#pragma warning(push)

// SbSocketWaiterPrivate is defined as a struct, but for windows implementation
// enough functionality has been added so that it warrants being a class
// per Google's C++ style guide.  This mismatch causes the Microsoft's compiler
// to generate a warning.
#pragma warning(disable : 4099)
class SbSocketWaiterPrivate {
 public:
  SbSocketWaiterPrivate();
  ~SbSocketWaiterPrivate();

  // These methods implement the SbSocketWaiter API defined in socket_waiter.h.
#if SB_API_VERSION >= 16 && !defined(_MSC_VER)
  bool Add(int socket,
           SbSocketWaiter waiter,
           void* context,
           SbPosixSocketWaiterCallback callback,
           int interests,
           bool persistent);
  bool Remove(int socket, SbSocketWaiter waiter);
  bool CheckSocketRegistered(int socket);
#endif  // SB_API_VERSION >= 16 && !defined(_MSC_VER)
  bool Add(SbSocket socket,
           void* context,
           SbSocketWaiterCallback callback,
           int interests,
           bool persistent);
  bool Remove(SbSocket socket);
  void Wait();
  SbSocketWaiterResult WaitTimed(int64_t duration_usec);
  void WakeUp();
  void HandleWakeUpRead();
  bool CheckSocketRegistered(SbSocket socket);

 private:
  // A registration of a socket with a socket waiter.
  struct Waitee {
#if SB_API_VERSION >= 16 && !defined(_MSC_VER)
    Waitee(SbSocketWaiter waiter,
           int socket,
           sbwin32::AutoEventHandle* socket_event_ptr,
           void* context,
           SbPosixSocketWaiterCallback callback,
           int interests,
           bool persistent)
        : waiter(waiter),
          posix_socket(socket),
          socket_event_ptr(socket_event_ptr),
          context(context),
          posix_callback(callback),
          interests(interests),
          persistent(persistent) {
      use_posix_socket = 1;
    }
    // The socket registered with the waiter.
    int posix_socket;

    // The callback to call when one or more registered interests become ready.
    SbPosixSocketWaiterCallback posix_callback;

    // The event related to the socket_handle.  Used for SbSocketWaiter.
    sbwin32::AutoEventHandle* socket_event_ptr;
#endif  // SB_API_VERSION >= 16 && !defined(_MSC_VER)
    Waitee(SbSocketWaiter waiter,
           SbSocket socket,
           void* context,
           SbSocketWaiterCallback callback,
           int interests,
           bool persistent)
        : waiter(waiter),
          socket(socket),
          context(context),
          callback(callback),
          interests(interests),
          persistent(persistent) {
      use_posix_socket = 0;
    }

    int use_posix_socket;

    // The socket registered with the waiter.
    SbSocket socket;

    // The callback to call when one or more registered interests become ready.
    SbSocketWaiterCallback callback;

    // The waiter this event is registered with.
    SbSocketWaiter waiter;

    // A context value that will be passed to the callback.
    void* context;

    // The set of interests registered with the waiter.
    int interests;

    // Whether this Waitee should stay registered after the next callback.
    bool persistent;
  };

  class WaiteeRegistry {
   public:
    typedef int64_t LookupToken;
    typedef std::deque<std::unique_ptr<Waitee>> Waitees;
#if SB_API_VERSION >= 16
    typedef std::unordered_map<int, std::size_t> posix_SocketToIndex;
#endif  // SB_API_VERSION >= 16 && !defined(_MSC_VER)
    typedef std::unordered_map<SbSocket, std::size_t> SocketToIndex;

    WSAEVENT* GetHandleArray() { return socket_events_.data(); }
    std::size_t GetHandleArraySize() { return socket_events_.size(); }
    const Waitees& GetWaitees() const { return waitees_; }

#if SB_API_VERSION >= 16
    // Gets the Waitee associated with the given socket, or nullptr.
    Waitee* GetWaitee(int socket);

    // Gets the index by socket
    starboard::optional<int64_t> GetIndex(int socket);

    // Returns true if socket was found, and removed.
    bool RemoveSocket(int socket);
#endif  // SB_API_VERSION >= 16 && !defined(_MSC_VER)
    // Gets the Waitee associated with the given socket, or nullptr.
    Waitee* GetWaitee(SbSocket socket);

    // Gets the index by socket
    starboard::optional<int64_t> GetIndex(SbSocket socket);

    // Returns true if socket was found, and removed.
    bool RemoveSocket(SbSocket socket);

    // Gets the Waitee by index.
    Waitee* GetWaiteeByIndex(LookupToken socket_index);

    // Returns the index of the event.
    LookupToken AddSocketEventAndWaitee(WSAEVENT socket_event,
                                        std::unique_ptr<Waitee> waitee);

   private:
#if SB_API_VERSION >= 16 && !defined(_MSC_VER)
    posix_SocketToIndex posix_socket_to_index_map_;
#endif  // SB_API_VERSION >= 16
    SocketToIndex socket_to_index_map_;

    std::vector<WSAEVENT> socket_events_;
    std::deque<std::unique_ptr<Waitee>> waitees_;
  };

  void SignalWakeupEvent();
  void ResetWakeupEvent();

#if SB_API_VERSION >= 16 && !defined(_MSC_VER)
  bool CheckSocketWaiterIsThis(int socket, SbSocketWaiter waiter);
#endif  // SB_API_VERSION >= 16
  bool CheckSocketWaiterIsThis(SbSocket socket);

  // The thread this waiter was created on. Immutable, so accessible from any
  // thread.
  const pthread_t thread_;

  // The registry of currently registered Waitees.
  WaiteeRegistry waitees_;
  WaiteeRegistry::LookupToken wakeup_event_token_;

  // This mutex covers the next two variables.
  starboard::Mutex unhandled_wakeup_count_mutex_;
  // Number of times wake up has been called, and not handled.
  std::int32_t unhandled_wakeup_count_;
  // The WSAEvent that is set by Wakeup();
  sbwin32::AutoEventHandle wakeup_event_;
};
#pragma warning(pop)

#endif  // STARBOARD_SHARED_WIN32_SOCKET_WAITER_INTERNAL_H_
