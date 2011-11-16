// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list_threadsafe.h"
#include "net/base/net_export.h"

namespace net {

class NetworkChangeNotifierFactory;

// NetworkChangeNotifier monitors the system for network changes, and notifies
// registered observers of those events.  Observers may register on any thread,
// and will be called back on the thread from which they registered.
// NetworkChangeNotifiers are threadsafe, though they must be created and
// destroyed on the same thread.
class NET_EXPORT NetworkChangeNotifier {
 public:
  class NET_EXPORT IPAddressObserver {
   public:
    virtual ~IPAddressObserver() {}

    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    IPAddressObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(IPAddressObserver);
  };

  class NET_EXPORT OnlineStateObserver {
   public:
    virtual ~OnlineStateObserver() {}

    // Will be called when the online state of the system may have changed.
    // See NetworkChangeNotifier::IsOffline() for important caveats about
    // the unreliability of this signal.
    virtual void OnOnlineStateChanged(bool online) = 0;

   protected:
    OnlineStateObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(OnlineStateObserver);
  };

  class NET_EXPORT DNSObserver {
   public:
    virtual ~DNSObserver() {}

    // Will be called when the DNS resolver of the system may have changed.
    // This is only used on Linux currently and watches /etc/resolv.conf
    // and /etc/hosts
    virtual void OnDNSChanged() = 0;

   protected:
    DNSObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DNSObserver);
  };

  virtual ~NetworkChangeNotifier();

  // See the description of NetworkChangeNotifier::IsOffline().
  // Implementations must be thread-safe. Implementations must also be
  // cheap as this could be called (repeatedly) from the IO thread.
  virtual bool IsCurrentlyOffline() const = 0;

  // Replaces the default class factory instance of NetworkChangeNotifier class.
  // The method will take over the ownership of |factory| object.
  static void SetFactory(NetworkChangeNotifierFactory* factory);

  // Creates the process-wide, platform-specific NetworkChangeNotifier.  The
  // caller owns the returned pointer.  You may call this on any thread.  You
  // may also avoid creating this entirely (in which case nothing will be
  // monitored), but if you do create it, you must do so before any other
  // threads try to access the API below, and it must outlive all other threads
  // which might try to use it.
  static NetworkChangeNotifier* Create();

  // Returns true if there is currently no internet connection.
  //
  // A return value of |true| is a pretty strong indicator that the user
  // won't be able to connect to remote sites. However, a return value of
  // |false| is inconclusive; even if some link is up, it is uncertain
  // whether a particular connection attempt to a particular remote site
  // will be successfully.
  static bool IsOffline();

  // Like Create(), but for use in tests.  The mock object doesn't monitor any
  // events, it merely rebroadcasts notifications when requested.
  static NetworkChangeNotifier* CreateMock();

  // Registers |observer| to receive notifications of network changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.  This is safe to call if Create() has not
  // been called (as long as it doesn't race the Create() call on another
  // thread), in which case it will simply do nothing.
  static void AddIPAddressObserver(IPAddressObserver* observer);
  static void AddOnlineStateObserver(OnlineStateObserver* observer);
  static void AddDNSObserver(DNSObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.  Like AddObserver(),
  // this is safe to call if Create() has not been called (as long as it doesn't
  // race the Create() call on another thread), in which case it will simply do
  // nothing.  Technically, it's also safe to call after the notifier object has
  // been destroyed, if the call doesn't race the notifier's destruction, but
  // there's no reason to use the API in this risky way, so don't do it.
  static void RemoveIPAddressObserver(IPAddressObserver* observer);
  static void RemoveOnlineStateObserver(OnlineStateObserver* observer);
  static void RemoveDNSObserver(DNSObserver* observer);

  // Allow unit tests to trigger notifications.
  static void NotifyObserversOfIPAddressChangeForTests() {
    NotifyObserversOfIPAddressChange();
  }

 protected:
  NetworkChangeNotifier();

  // Broadcasts a notification to all registered observers.  Note that this
  // happens asynchronously, even for observers on the current thread, even in
  // tests.
  static void NotifyObserversOfIPAddressChange();
  static void NotifyObserversOfOnlineStateChange();
  static void NotifyObserversOfDNSChange();

 private:
  friend class NetworkChangeNotifierWinTest;

  // Allows a second NetworkChangeNotifier to be created for unit testing, so
  // the test suite can create a MockNetworkChangeNotifier, but platform
  // specific NetworkChangeNotifiers can also be created for testing.  To use,
  // create an DisableForTest object, and then create the new
  // NetworkChangeNotifier object.  The NetworkChangeNotifier must be
  // destroyed before the DisableForTest object, as its destruction will restore
  // the original NetworkChangeNotifier.
  class NET_EXPORT_PRIVATE DisableForTest {
   public:
    DisableForTest();
    ~DisableForTest();

   private:
    // The original NetworkChangeNotifier to be restored on destruction.
    NetworkChangeNotifier* network_change_notifier_;
  };

  const scoped_refptr<ObserverListThreadSafe<IPAddressObserver> >
      ip_address_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<OnlineStateObserver> >
      online_state_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<DNSObserver> >
      resolver_state_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
