// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/base/network_change_notifier_linux.h"
#elif defined(OS_MACOSX)
#include "net/base/network_change_notifier_mac.h"
#endif

namespace net {

namespace {

// The actual singleton notifier.  The class contract forbids usage of the API
// in ways that would require us to place locks around access to this object.
// (The prohibition on global non-POD objects makes it tricky to do such a thing
// anyway.)
NetworkChangeNotifier* g_network_change_notifier = NULL;

// Class factory singleton.
NetworkChangeNotifierFactory* g_network_change_notifier_factory = NULL;

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  virtual bool IsCurrentlyOffline() const { return false; }
};

}  // namespace

NetworkChangeNotifier::~NetworkChangeNotifier() {
  DCHECK_EQ(this, g_network_change_notifier);
  g_network_change_notifier = NULL;
}

// static
void NetworkChangeNotifier::SetFactory(
    NetworkChangeNotifierFactory* factory) {
  CHECK(!g_network_change_notifier_factory);
  g_network_change_notifier_factory = factory;
}

// static
NetworkChangeNotifier* NetworkChangeNotifier::Create() {
  if (g_network_change_notifier_factory)
    return g_network_change_notifier_factory->CreateInstance();

#if defined(OS_WIN)
  NetworkChangeNotifierWin* network_change_notifier =
      new NetworkChangeNotifierWin();
  network_change_notifier->WatchForAddressChange();
  return network_change_notifier;
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // ChromeOS and Android builds MUST use their own class factory.
#if !defined(OS_CHROMEOS)
  // TODO(oshima): ash_shell do not have access to chromeos'es
  // notifier yet. Re-enable this when chromeos'es notifier moved to
  // chromeos root directory. crbug.com/119298.
  CHECK(false);
#endif
  return NULL;
#elif defined(OS_LINUX)
  return NetworkChangeNotifierLinux::Create();
#elif defined(OS_MACOSX)
  return new NetworkChangeNotifierMac();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

// static
bool NetworkChangeNotifier::IsOffline() {
  return g_network_change_notifier &&
         g_network_change_notifier->IsCurrentlyOffline();
}

// static
bool NetworkChangeNotifier::IsWatchingDNS() {
  if (!g_network_change_notifier)
    return false;
  base::AutoLock(g_network_change_notifier->watching_dns_lock_);
  return g_network_change_notifier->watching_dns_;
}

// static
NetworkChangeNotifier* NetworkChangeNotifier::CreateMock() {
  return new MockNetworkChangeNotifier();
}

void NetworkChangeNotifier::AddIPAddressObserver(IPAddressObserver* observer) {
  if (g_network_change_notifier)
    g_network_change_notifier->ip_address_observer_list_->AddObserver(observer);
}

void NetworkChangeNotifier::AddOnlineStateObserver(
    OnlineStateObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->online_state_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::AddDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveIPAddressObserver(
    IPAddressObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveOnlineStateObserver(
    OnlineStateObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->online_state_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->RemoveObserver(
        observer);
  }
}

NetworkChangeNotifier::NetworkChangeNotifier()
    : ip_address_observer_list_(
        new ObserverListThreadSafe<IPAddressObserver>(
            ObserverListBase<IPAddressObserver>::NOTIFY_EXISTING_ONLY)),
      online_state_observer_list_(
        new ObserverListThreadSafe<OnlineStateObserver>(
            ObserverListBase<OnlineStateObserver>::NOTIFY_EXISTING_ONLY)),
      resolver_state_observer_list_(
        new ObserverListThreadSafe<DNSObserver>(
            ObserverListBase<DNSObserver>::NOTIFY_EXISTING_ONLY)),
      watching_dns_(false) {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
}

// static
void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->Notify(
        &IPAddressObserver::OnIPAddressChanged);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfDNSChange(unsigned detail) {
  if (g_network_change_notifier) {
    {
      base::AutoLock(g_network_change_notifier->watching_dns_lock_);
      if (detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_STARTED) {
        g_network_change_notifier->watching_dns_ = true;
      } else if (detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED) {
        g_network_change_notifier->watching_dns_ = false;
      }
      // Include detail that watch is off to spare the call to IsWatchingDNS.
      if (!g_network_change_notifier->watching_dns_)
        detail |= NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED;
    }
    DCHECK(!(detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED) ||
           !(detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_STARTED));
    g_network_change_notifier->resolver_state_observer_list_->Notify(
        &DNSObserver::OnDNSChanged, detail);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfOnlineStateChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->online_state_observer_list_->Notify(
        &OnlineStateObserver::OnOnlineStateChanged, !IsOffline());
  }
}

NetworkChangeNotifier::DisableForTest::DisableForTest()
    : network_change_notifier_(g_network_change_notifier) {
  DCHECK(g_network_change_notifier);
  g_network_change_notifier = NULL;
}

NetworkChangeNotifier::DisableForTest::~DisableForTest() {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = network_change_notifier_;
}

}  // namespace net
