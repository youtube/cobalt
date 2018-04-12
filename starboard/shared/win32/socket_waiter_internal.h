// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/common/optional.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/win32/auto_event_handle.h"
#include "starboard/shared/win32/socket_internal.h"
#include "starboard/socket.h"
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
  bool Add(SbSocket socket,
           void* context,
           SbSocketWaiterCallback callback,
           int interests,
           bool persistent);
  bool Remove(SbSocket socket);
  void Wait();
  SbSocketWaiterResult WaitTimed(SbTime duration);
  void WakeUp();
  void HandleWakeUpRead();

 private:
  // A registration of a socket with a socket waiter.
  struct Waitee {
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
          persistent(persistent) {}
    // The waiter this event is registered with.
    SbSocketWaiter waiter;

    // The socket registered with the waiter.
    SbSocket socket;

    // A context value that will be passed to the callback.
    void* context;

    // The callback to call when one or more registered interests become ready.
    SbSocketWaiterCallback callback;

    // The set of interests registered with the waiter.
    int interests;

    // Whether this Waitee should stay registered after the next callback.
    bool persistent;
  };

  class WaiteeRegistry {
   public:
    typedef int64_t LookupToken;
    typedef std::deque<std::unique_ptr<Waitee>> Waitees;
    typedef std::unordered_map<SbSocket, std::size_t> SocketToIndex;

    WSAEVENT* GetHandleArray() { return socket_events_.data(); }
    std::size_t GetHandleArraySize() { return socket_events_.size(); }
    const Waitees& GetWaitees() const { return waitees_; }

    // Gets the Waitee associated with the given socket, or nullptr.
    Waitee* GetWaitee(SbSocket socket);

    // Gets the index by socket
    starboard::optional<int64_t> GetIndex(SbSocket socket);

    // Gets the Waitee by index.
    Waitee* GetWaiteeByIndex(LookupToken socket_index);

    // Returns the index of the event.
    LookupToken AddSocketEventAndWaitee(WSAEVENT socket_event,
                                        std::unique_ptr<Waitee> waitee);
    // Returns true if socket was found, and removed.
    bool RemoveSocket(SbSocket socket);

   private:
    SocketToIndex socket_to_index_map_;
    std::vector<WSAEVENT> socket_events_;
    std::deque<std::unique_ptr<Waitee>> waitees_;
  };

  void SignalWakeupEvent();
  void ResetWakeupEvent();

  bool CheckSocketWaiterIsThis(SbSocket socket);

  // The thread this waiter was created on. Immutable, so accessible from any
  // thread.
  const SbThread thread_;

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
