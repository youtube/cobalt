// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_OBSERVER_REGISTRY_H_
#define STARBOARD_TVOS_SHARED_OBSERVER_REGISTRY_H_

#include <atomic>

namespace starboard {

// This module helps ensure thread safety of notification observers which may
// be destroyed from a different thread than which the observer callback is
// invoked. The main problem is that the observer callback may be in progress
// when the observer is destroyed, so synchronization is needed when
// unregistering the observer.
class ObserverRegistry {
 public:
  // Observers should contain an |Observer| object to interact with the
  // registry. This object serves to identify observers uniquely by address and
  // id; doing so hardens the system against situations where observers may be
  // freed then another allocated at the same address.
  //
  // Example use:
  //   class MyObserver {
  //     public:
  //       MyObserver() {
  //         ObserverRegistry::RegisterObserver(&observer_);
  //         ... register |this| as observer with notification center ...
  //       }
  //       ~MyObserver() {
  //         ... unregister |this| from notification center ...
  //         ObserverRegistry::UnregisterObserver(&observer_);
  //       }
  //       static void NotificationCallback(CFNotificationCenterRef center,
  //         void *observer, ...) {
  //         MyObserver* pThis = static_cast<MyObserver*>(observer);
  //         int32_t lock_slot = ObserverRegistry::LockObserver(
  //             &pThis->observer_);
  //         if (lock_slot < 0) {
  //           return;  // Observer was already unregistered.
  //         }
  //         ... do stuff with |observer| ...
  //         ObserverRegistry::UnlockObserver(lock_slot);
  //       }
  //     private:
  //       ObserverRegistry::Observer observer_;
  //   };
  //
  // NOTE: It is key that |&pThis->observer_| is used in the notification
  // callback when trying to lock the observer. This expression only does
  // pointer arithmetic and does not actually dereference |pThis| which may
  // be deallocated already.
  struct Observer final {
   public:
    Observer() : id(id_counter.fetch_add(1, std::memory_order_relaxed)) {}
    const int32_t id;

   private:
    static volatile std::atomic_int32_t id_counter;
  };

  // Observers should be registered with this module before they are added to
  // the notification center.
  static void RegisterObserver(const Observer* observer);

  // Observers should be unregistered before they are destroyed. Once this call
  // returns, it is guaranteed that no observer lock is held and no subsequent
  // observer locks will succeed. This should not be called with the observer
  // locked.
  static void UnregisterObserver(const Observer* observer);

  // The notification callback should lock the relevant observer before
  // proceeding. If the observer has not been unregistered, then this returns
  // a lock slot that is >= 0; this should be used with UnlockObserver().
  // Otherwise, the observer should be considered invalid, and a negative value
  // is returned.
  static int32_t LockObserver(const Observer* observer);

  // The notification callback should unlock the observer once the callback
  // is done operating on the observer. This should only be done if LockObserver
  // returned a lock slot >= 0.
  static void UnlockObserver(int32_t lock_slot);
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_OBSERVER_REGISTRY_H_
