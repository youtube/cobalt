// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX) || defined(OS_ANDROID)
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
#elif defined(OS_CHROMEOS)
  // ChromeOS builds MUST use its own class factory.
  CHECK(false);
  return NULL;
#elif defined(OS_LINUX) || defined(OS_ANDROID)
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
            ObserverListBase<DNSObserver>::NOTIFY_EXISTING_ONLY)) {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
}

void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->Notify(
        &IPAddressObserver::OnIPAddressChanged);
  }
}

void NetworkChangeNotifier::NotifyObserversOfDNSChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->Notify(
        &DNSObserver::OnDNSChanged);
  }
}

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
