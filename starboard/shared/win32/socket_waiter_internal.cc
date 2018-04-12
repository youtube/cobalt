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

#include "starboard/shared/win32/socket_waiter_internal.h"

#include <windows.h>

#include <algorithm>

#include "starboard/common/optional.h"
#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/socket_internal.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/time_utils.h"
#include "starboard/thread.h"

namespace sbwin32 = starboard::shared::win32;

namespace {

// This is a helper function that takes data from |network_events|, and then
// adds the bitwise ORs |interest_to_add| onto |interest_out|.
// For more information, please see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms741572(v=vs.85).aspx.
void TranslateNetworkBitIntoInterests(const WSANETWORKEVENTS& network_events,
                                      int bit_to_check,
                                      SbSocketWaiterInterest interest_to_add,
                                      SbSocketWaiterInterest* interests_out) {
  SB_DCHECK(interests_out);

  static_assert(
      sizeof(SbSocketWaiterInterest) == sizeof(int),
      "Assuming size of enum is size of int, due to the bitfield logic below.");

  if (network_events.lNetworkEvents & (1 << bit_to_check)) {
    *(reinterpret_cast<int*>(interests_out)) |=
        static_cast<int>(interest_to_add);
    const int error_code = network_events.iErrorCode[bit_to_check];
    if (error_code != 0) {
      SB_DLOG(ERROR) << "Error on network event " << bit_to_check << " "
                     << sbwin32::Win32ErrorCode(error_code);
    }
  }
}

SbSocketWaiterInterest DiscoverNetworkEventInterests(SOCKET socket_handle) {
  // Please take note that WSAEnumNetworkEvents below only works with
  // WSAEventSelect.
  SbSocketWaiterInterest interests = kSbSocketWaiterInterestNone;
  WSANETWORKEVENTS network_events = {0};
  int return_code =
      WSAEnumNetworkEvents(socket_handle, nullptr, &network_events);
  if (return_code == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "WSAEnumNetworkEvents failed with last_error = "
                   << sbwin32::Win32ErrorCode(last_error);
    return interests;
  }

  // Translate information from WSAEnumNetworkEvents to interests:
  // From the MSDN documentation:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms741572(v=vs.85).aspx
  // The lNetworkEvents member of the WSANETWORKEVENTS structure indicates which
  // of the FD_XXX network events have occurred. The iErrorCode array is used to
  // contain any associated error codes with the array index corresponding to
  // the position of event bits in lNetworkEvents. Identifiers such as
  // FD_READ_BIT and FD_WRITE_BIT can be used to index the iErrorCode array.
  // Note that only those elements of the iErrorCode array are set that
  // correspond to the bits set in lNetworkEvents parameter
  TranslateNetworkBitIntoInterests(network_events, FD_READ_BIT,
                                   kSbSocketWaiterInterestRead, &interests);
  TranslateNetworkBitIntoInterests(network_events, FD_ACCEPT_BIT,
                                   kSbSocketWaiterInterestRead, &interests);
  TranslateNetworkBitIntoInterests(network_events, FD_CLOSE_BIT,
                                   kSbSocketWaiterInterestRead, &interests);

  TranslateNetworkBitIntoInterests(network_events, FD_CONNECT_BIT,
                                   kSbSocketWaiterInterestWrite, &interests);
  TranslateNetworkBitIntoInterests(network_events, FD_WRITE_BIT,
                                   kSbSocketWaiterInterestWrite, &interests);

  return interests;
}

// The function erases the |index|th element from the collection by swapping
// it with the last element.  This operation leaves all the other elements in
// place, which is useful for some operations.
template <typename T>
void EraseIndexFromVector(T* collection_pointer, std::size_t index) {
  SB_DCHECK(collection_pointer);
  T& collection = *collection_pointer;
  const std::size_t current_size = collection.size();
  if (current_size <= 1) {
    collection.clear();
    return;
  }
  const std::size_t new_size = collection.size() - 1;
  std::swap(collection[index], collection[new_size]);
  collection.resize(new_size);
}

SbSocketWaiterInterest CombineInterests(
    SbSocketWaiterInterest a, SbSocketWaiterInterest b) {
  int a_int = static_cast<int>(a);
  int b_int = static_cast<int>(b);
  return static_cast<SbSocketWaiterInterest>(a_int | b_int);
}

}  // namespace

SbSocketWaiterPrivate::SbSocketWaiterPrivate()
    : thread_(SbThreadGetCurrent()),
      wakeup_event_token_(-1),
      wakeup_event_(CreateEvent(nullptr, false, false, nullptr)) {
  {
    starboard::ScopedLock lock(unhandled_wakeup_count_mutex_);
    unhandled_wakeup_count_ = 0;
  }
  if (wakeup_event_.IsValid() == false) {
    SB_DLOG(ERROR) << "Could not create wakeup event: "
                   << starboard::shared::win32::Win32ErrorCode(GetLastError());
    return;
  }
  wakeup_event_token_ =
      waitees_.AddSocketEventAndWaitee(wakeup_event_.GetEvent(), nullptr);
}

SbSocketWaiterPrivate::~SbSocketWaiterPrivate() {
  for (auto& it : waitees_.GetWaitees()) {
    if (it) {
      SB_DCHECK(CheckSocketWaiterIsThis(it->socket));
    }
  }
}

bool SbSocketWaiterPrivate::Add(SbSocket socket,
                                void* context,
                                SbSocketWaiterCallback callback,
                                int interests,
                                bool persistent) {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") is invalid.";
    return false;
  }

  if (!interests) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No interests provided.";
    return false;
  }

  // The policy is not to add a socket to a waiter if it is registered with
  // another waiter.

  // TODO: If anyone were to want to add a socket to a different waiter,
  // it would probably be another thread, so doing this check without locking is
  // probably wrong. But, it is also a pain, and, at this precise moment, socket
  // access is all going to come from one I/O thread anyway, and there will only
  // be one waiter.
  if (SbSocketWaiterIsValid(socket->waiter)) {
    if (socket->waiter == this) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Socket already has this waiter ("
                     << this << ").";
    } else {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Socket already has waiter ("
                     << socket->waiter << ", this=" << this << ").";
    }
    return false;
  }

  int network_event_interests = 0;
  if (interests & kSbSocketWaiterInterestRead) {
    network_event_interests |= FD_READ | FD_ACCEPT | FD_CLOSE;
  }

  if (interests & kSbSocketWaiterInterestWrite) {
    network_event_interests |= FD_CONNECT | FD_WRITE;
  }

  const BOOL manual_reset = !persistent;

  SB_DCHECK(!socket->socket_event.IsValid());

  socket->socket_event.Reset(
      CreateEvent(nullptr, manual_reset, false, nullptr));
  if (socket->socket_event.GetEvent() == WSA_INVALID_EVENT) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "Error calling WSACreateEvent() last_error = "
                   << sbwin32::Win32ErrorCode(last_error);
    return false;
  }

  // Note that WSAEnumNetworkEvents used elsewhere only works with
  // WSAEventSelect.
  // Please consider that before changing this code.
  int return_value =
      WSAEventSelect(socket->socket_handle, socket->socket_event.GetEvent(),
                     network_event_interests);

  if (return_value == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "Error calling WSAEventSelect() last_error = "
                   << sbwin32::Win32ErrorCode(last_error);
    return false;
  }

  if (waitees_.GetHandleArraySize() >= MAXIMUM_WAIT_OBJECTS) {
    SB_DLOG(ERROR) << "Reached maxed number of socket events ("
                   << MAXIMUM_WAIT_OBJECTS << ")";
    return false;
  }

  std::unique_ptr<Waitee> waitee(
      new Waitee(this, socket, context, callback, interests, persistent));
  waitees_.AddSocketEventAndWaitee(socket->socket_event.GetEvent(),
                                   std::move(waitee));
  socket->waiter = this;

  return true;
}

bool SbSocketWaiterPrivate::Remove(SbSocket socket) {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  if (!CheckSocketWaiterIsThis(socket)) {
    return false;
  }

  socket->waiter = kSbSocketWaiterInvalid;

  socket->socket_event.Reset(nullptr);
  return waitees_.RemoveSocket(socket);
}

void SbSocketWaiterPrivate::HandleWakeUpRead() {
  SB_LOG(INFO) << "HandleWakeUpRead incrementing counter..";
  starboard::ScopedLock lock(unhandled_wakeup_count_mutex_);
  ++unhandled_wakeup_count_;
}

void SbSocketWaiterPrivate::SignalWakeupEvent() {
  SB_DCHECK(wakeup_event_.IsValid());
  WSASetEvent(wakeup_event_.GetEvent());
}

void SbSocketWaiterPrivate::ResetWakeupEvent() {
  SB_DCHECK(wakeup_event_.IsValid());
  WSAResetEvent(wakeup_event_.GetEvent());
}

bool SbSocketWaiterPrivate::CheckSocketWaiterIsThis(SbSocket socket) {
  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") is invalid.";
    return false;
  }

  if (socket->waiter != this) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") "
                   << "is watched by Waiter (" << socket->waiter << "), "
                   << "not this Waiter (" << this << ").";
    SB_DSTACK(ERROR);
    return false;
  }

  return true;
}

void SbSocketWaiterPrivate::Wait() {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  // We basically wait for the largest amount of time to achieve an indefinite
  // block.
  WaitTimed(kSbTimeMax);
}

SbSocketWaiterResult SbSocketWaiterPrivate::WaitTimed(SbTime duration) {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  const SbTimeMonotonic start_time = SbTimeGetMonotonicNow();
  int64_t duration_left = duration;

  while (true) {
    // |waitees_| could have been modified in the last loop iteration, so
    // re-read it.
    const DWORD number_events =
        static_cast<DWORD>(waitees_.GetHandleArraySize());

    const DWORD millis = sbwin32::ConvertSbTimeToMillisRoundUp(duration_left);

    {
      starboard::ScopedLock lock(unhandled_wakeup_count_mutex_);
      if (unhandled_wakeup_count_ > 0) {
        --unhandled_wakeup_count_;
        // The signaling thread also set the event, so reset it.
        ResetWakeupEvent();
        return kSbSocketWaiterResultWokenUp;
      }
    }

    // There should always be a wakeup event.
    SB_DCHECK(number_events > 0);

    SbSocket maybe_writable_socket = kSbSocketInvalid;
    for (auto& it : waitees_.GetWaitees()) {
      if (!it) {
        continue;
      }
      if ((it->interests & kSbSocketWaiterInterestWrite) == 0) {
        continue;
      }
      if (it->socket->writable.load()) {
        maybe_writable_socket = it->socket;
        break;
      }
    }

    bool has_writable = (maybe_writable_socket != kSbSocketInvalid);
    DWORD return_value = WSAWaitForMultipleEvents(
        number_events, waitees_.GetHandleArray(),
        false, has_writable ? 0 : millis, false);

    if (has_writable || ((return_value >= WSA_WAIT_EVENT_0) &&
        (return_value < (WSA_WAIT_EVENT_0 + number_events)))) {
      int64_t socket_index;
      if (has_writable) {
        socket_index = waitees_.GetIndex(maybe_writable_socket).value();
      } else {
        socket_index = static_cast<int64_t>(return_value) -
                       static_cast<int64_t>(WSA_WAIT_EVENT_0);
      }
      SB_DCHECK(socket_index >= 0);
      if (socket_index < 0) {
        SB_NOTREACHED() << "Bad socket_index. " << socket_index;
        return kSbSocketWaiterResultTimedOut;
      }

      // Make sure wakeup_event_token_ is initialized.
      SB_DCHECK(wakeup_event_token_ >= 0);

      if (socket_index == wakeup_event_token_) {
        starboard::ScopedLock lock(unhandled_wakeup_count_mutex_);
        SB_DCHECK(unhandled_wakeup_count_ > 0);

        --unhandled_wakeup_count_;
        // This was a dummy event.  We were woken up.
        // Note that we do not need to reset the event here,
        // since it was created using an auto-reset flag.
        return kSbSocketWaiterResultWokenUp;
      } else {
        Waitee* waitee = waitees_.GetWaiteeByIndex(socket_index);

        // Remove the non-persistent waitee before calling the callback, so
        // that we can add another waitee in the callback if we need to. This
        // is also why we copy all the fields we need out of waitee.
        const SbSocket socket = waitee->socket;
        const SbSocketWaiterCallback callback = waitee->callback;
        void* context = waitee->context;

        // Note: this should also go before Remove().
        SbSocketWaiterInterest interests =
            DiscoverNetworkEventInterests(socket->socket_handle);

        if ((waitee->interests & kSbSocketWaiterInterestWrite) &&
              socket->writable.load()) {
          interests = CombineInterests(interests, kSbSocketWaiterInterestWrite);
        } else if (interests & kSbSocketWaiterInterestWrite) {
          socket->writable.store(true);
        }

        if (!waitee->persistent) {
          Remove(waitee->socket);
        }

        callback(this, socket, context, interests);
      }
    } else if (return_value == WSA_WAIT_FAILED) {
      SB_DLOG(ERROR) << "Wait failed -- "
                     << sbwin32::Win32ErrorCode(WSAGetLastError());
      return kSbSocketWaiterResultInvalid;
    } else if (return_value == WSA_WAIT_TIMEOUT) {
      // Do nothing, check time ourselves.
    } else {
      SB_NOTREACHED() << "Unhandled case: " << return_value;
      return kSbSocketWaiterResultInvalid;
    }

    const int64_t time_elapsed =
        static_cast<std::int64_t>(SbTimeGetMonotonicNow()) -
        static_cast<std::int64_t>(start_time);
    duration_left -= time_elapsed;
    if (duration_left < 0) {
      return kSbSocketWaiterResultTimedOut;
    }
  }

  SB_NOTREACHED() << "Invalid state reached";
  return kSbSocketWaiterResultInvalid;
}

void SbSocketWaiterPrivate::WakeUp() {
  // Increasing unhandled_wakeup_count_mutex_ and calling SignalWakeupEvent
  // atomically helps add additional guarantees of when the waiter can be
  // woken up.  While we can code around this easily, having a stronger
  // coupling enables us to add DCHECKs for |unhandled_wakeup_count_| in other
  // parts of the code.
  starboard::ScopedLock lock(unhandled_wakeup_count_mutex_);
  ++unhandled_wakeup_count_;
  SignalWakeupEvent();
}

SbSocketWaiterPrivate::Waitee* SbSocketWaiterPrivate::WaiteeRegistry::GetWaitee(
    SbSocket socket) {
  starboard::optional<int64_t> token = GetIndex(socket);
  if (!token) {
    return nullptr;
  }
  return waitees_[token.value()].get();
}

starboard::optional<int64_t>
SbSocketWaiterPrivate::WaiteeRegistry::GetIndex(SbSocket socket) {
  auto iterator = socket_to_index_map_.find(socket);
  if (iterator == socket_to_index_map_.end()) {
    return starboard::nullopt;
  }
  return iterator->second;
}

SbSocketWaiterPrivate::WaiteeRegistry::LookupToken
SbSocketWaiterPrivate::WaiteeRegistry::AddSocketEventAndWaitee(
    WSAEVENT socket_event,
    std::unique_ptr<Waitee> waitee) {
  SB_DCHECK(socket_event != WSA_INVALID_EVENT);
  SB_DCHECK(socket_events_.size() == waitees_.size());
  SbSocket socket = kSbSocketInvalid;
  if (waitee) {
    socket = waitee->socket;
  }
  socket_to_index_map_.emplace(socket, socket_events_.size());
  socket_events_.emplace_back(socket_event);
  waitees_.emplace_back(std::move(waitee));

  return socket_events_.size() - 1;
}

bool SbSocketWaiterPrivate::WaiteeRegistry::RemoveSocket(SbSocket socket) {
  auto iterator = socket_to_index_map_.find(socket);
  if (iterator == socket_to_index_map_.end()) {
    return false;
  }

  const std::size_t current_size = socket_events_.size();
  SB_DCHECK(current_size == waitees_.size());

  const std::size_t socket_index = iterator->second;
  SbSocket socket_to_swap = waitees_[current_size - 1]->socket;

  // Since |EraseIndexFromVector| will swap the last socket and the socket
  // at current index, |socket_to_index_| will need to be updated.
  socket_to_index_map_[socket_to_swap] = socket_index;
  // Note that |EraseIndexFromVector| only touches the last element and the
  // element to remove.
  EraseIndexFromVector(&socket_events_, socket_index);
  EraseIndexFromVector(&waitees_, socket_index);

  socket_to_index_map_.erase(socket);

  SB_DCHECK(socket_events_.size() == waitees_.size());
  SB_DCHECK(socket_events_.size() == socket_to_index_map_.size());
  return true;
}

SbSocketWaiterPrivate::Waitee*
SbSocketWaiterPrivate::WaiteeRegistry::GetWaiteeByIndex(
    const SbSocketWaiterPrivate::WaiteeRegistry::LookupToken socket_index) {
  SB_DCHECK(socket_index >= 0);

  SB_DCHECK(static_cast<std::size_t>(socket_index) <= socket_events_.size());
  return waitees_[socket_index].get();
}
