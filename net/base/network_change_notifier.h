// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_

#include "base/basictypes.h"
#include "base/observer_list_threadsafe.h"

namespace net {

// NetworkChangeNotifier monitors the system for network changes, and notifies
// registered observers of those events.  Observers may register on any thread,
// and will be called back on the thread from which they registered.
class NetworkChangeNotifier {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~NetworkChangeNotifier();

  // Creates the process-wide, platform-specific NetworkChangeNotifier.  The
  // caller owns the returned pointer.  You may call this on any thread.  You
  // may also avoid creating this entirely (in which case nothing will be
  // monitored), but if you do create it, you must do so before any other
  // threads try to access the API below, and it must outlive all other threads
  // which might try to use it.
  static NetworkChangeNotifier* Create();

#ifdef UNIT_TEST
  // Like Create(), but for use in tests.  The mock object doesn't monitor any
  // events, it merely rebroadcasts notifications when requested.
  static NetworkChangeNotifier* CreateMock() {
    return new NetworkChangeNotifier();
  }
#endif

  // Registers |observer| to receive notifications of network changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.  This is safe to call if Create() has not
  // been called (as long as it doesn't race the Create() call on another
  // thread), in which case it will simply do nothing.
  static void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.  Like AddObserver(),
  // this is safe to call if Create() has not been called (as long as it doesn't
  // race the Create() call on another thread), in which case it will simply do
  // nothing.  Technically, it's also safe to call after the notifier object has
  // been destroyed, if the call doesn't race the notifier's destruction, but
  // there's no reason to use the API in this risky way, so don't do it.
  static void RemoveObserver(Observer* observer);

#ifdef UNIT_TEST
  // Allow unit tests to trigger notifications.
  static void NotifyObserversOfIPAddressChangeForTests() {
    NotifyObserversOfIPAddressChange();
  }
#endif

 protected:
  NetworkChangeNotifier();

  // Broadcasts a notification to all registered observers.  Note that this
  // happens asynchronously, even for observers on the current thread, even in
  // tests.
  static void NotifyObserversOfIPAddressChange();

 private:
  const scoped_refptr<ObserverListThreadSafe<Observer> > observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
