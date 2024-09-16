// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LIBEVENT_SOCKET_WAITER_INTERNAL_H_
#define STARBOARD_SHARED_LIBEVENT_SOCKET_WAITER_INTERNAL_H_

#include <pthread.h>

#include <map>

#include "starboard/common/socket.h"
#include "starboard/shared/internal_only.h"
#include "starboard/socket_waiter.h"
#include "starboard/types.h"
#include "third_party/libevent/event.h"

struct SbSocketWaiterPrivate {
 public:
  SbSocketWaiterPrivate();
  ~SbSocketWaiterPrivate();

  // These methods implement the SbSocketWaiter API defined in socket_waiter.h.

  // The Add/Remove pair for integer based socket
  bool Add(int socket,
           SbSocketWaiter waiter,
           void* context,
           SbPosixSocketWaiterCallback callback,
           int interests,
           bool persistent);
  bool Remove(int socket, SbSocketWaiter waiter);
  bool CheckSocketRegistered(int socket);

  // The Add/Remove pair for SbSocket based socket
  bool Add(SbSocket socket,
           void* context,
           SbSocketWaiterCallback callback,
           int interests,
           bool persistent);
  bool Remove(SbSocket socket, SbSocketWaiter waiter);
  bool CheckSocketRegistered(SbSocket socket);

  void Wait();
  SbSocketWaiterResult WaitTimed(int64_t duration_usec);
  void WakeUp(bool timeout);

  int use_int_socket;

 private:
  // A registration of a socket with a socket waiter.
  struct Waitee {
    Waitee(SbSocketWaiter waiter,
           int socket,
           void* context,
           SbPosixSocketWaiterCallback callback,
           int interests,
           bool persistent)
        : waiter(waiter),
          i_socket(socket),
          context(context),
          i_callback(callback),
          interests(interests),
          persistent(persistent) {
      use_int_socket = 1;
    }
    Waitee(SbSocketWaiter waiter,
           SbSocket socket,
           void* context,
           SbSocketWaiterCallback callback,
           int interests,
           bool persistent)
        : waiter(waiter),
          sb_socket(socket),
          context(context),
          sb_callback(callback),
          interests(interests),
          persistent(persistent) {
      use_int_socket = 0;
    }

    // The socket registered with the waiter.
    int i_socket;
    SbSocket sb_socket;
    int use_int_socket;

    // The callback to call when one or more registered interests become ready.
    SbPosixSocketWaiterCallback i_callback;

    SbSocketWaiterCallback sb_callback;

    // The waiter this event is registered with.
    SbSocketWaiter waiter;

    // A context value that will be passed to the callback.
    void* context;

    // The set of interests registered with the waiter.
    int interests;

    // Whether this Waitee should stay registered after the next callback.
    bool persistent;

    // The libevent event backing this Waitee.
    struct event event;
  };

  // The way Waitees are stored, indexed by SbSocket.
  //
  // NOTE: This is a (tree) map because we don't have base::hash_map here. We
  // should keep an eye out for whether this is a performance issue.
  typedef std::map<int, Waitee*> i_WaiteesMap;
  typedef std::map<SbSocket, Waitee*> sb_WaiteesMap;

  // The libevent callback function, which in turn calls the registered callback
  // function for the Waitee.
  static void LibeventSocketCallback(int fd, int16_t events, void* context);

  // A libevent callback function that wakes up the SbSocketWaiter given in
  // |context| due to a timeout.
  static void LibeventTimeoutCallback(int fd, int16_t event, void* context);

  // A libevent callback function that wakes up the SbSocketWaiter given in
  // |context| due to an external call to WakeUp.
  static void LibeventWakeUpCallback(int fd, int16_t event, void* context);

  // Handles a libevent callback.
  void HandleSignal(Waitee* waitee, short events);  // NOLINT[runtime/int]

  // Handles a libevent callback on the wakeup_read_fd_.
  void HandleWakeUpRead();

  // Adds |waitee| to the waitee registry.
  void AddWaitee(Waitee* waitee);

  // Gets the Waitee associated with the given socket, or NULL.
  Waitee* GetWaitee(int socket);

  // Gets the Waitee associated with the given socket, removing it from the
  // registry, or NULL.
  Waitee* RemoveWaitee(int socket) SB_WARN_UNUSED_RESULT;

  // Gets the Waitee associated with the given socket, or NULL.
  Waitee* GetWaitee(SbSocket socket);

  // Gets the Waitee associated with the given socket, removing it from the
  // registry, or NULL.
  Waitee* RemoveWaitee(SbSocket socket) SB_WARN_UNUSED_RESULT;

  // The thread this waiter was created on. Immutable, so accessible from any
  // thread.
  const pthread_t thread_;

  // The libevent event_base backing this waiter. Immutable, so accessible from
  // any thread.
  struct event_base* const base_;

  // A file descriptor of the write end of a pipe that can be written to from
  // any thread to wake up this waiter in a thread-safe manner.
  int wakeup_write_fd_;

  // A file descriptor of the read end of a pipe that this waiter will wait on
  // to allow it to be signalled safely from other threads.
  int wakeup_read_fd_;

  // The libevent event that waits on |wakeup_read_fd_| for WakeUp signals.
  struct event wakeup_event_;

  // The registry of currently registered Waitees.
  i_WaiteesMap i_waitees_;
  sb_WaiteesMap sb_waitees_;

  // Whether or not the waiter is actually waiting.
  bool waiting_;

  // Whether or not the waiter was woken up.
  bool woken_up_;

  // Used to replace pipe.
  SbSocket server_socket_;
  SbSocket client_socket_;
};

#endif  // STARBOARD_SHARED_LIBEVENT_SOCKET_WAITER_INTERNAL_H_
