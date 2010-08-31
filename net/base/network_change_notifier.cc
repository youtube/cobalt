// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX)
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

}  // namespace

NetworkChangeNotifier::~NetworkChangeNotifier() {
  DCHECK_EQ(this, g_network_change_notifier);
  g_network_change_notifier = NULL;
}

NetworkChangeNotifier* NetworkChangeNotifier::Create() {
#if defined(OS_WIN)
  return new NetworkChangeNotifierWin();
#elif defined(OS_LINUX)
  return new NetworkChangeNotifierLinux();
#elif defined(OS_MACOSX)
  return new NetworkChangeNotifierMac();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

void NetworkChangeNotifier::AddObserver(Observer* observer) {
  if (g_network_change_notifier)
    g_network_change_notifier->observer_list_->AddObserver(observer);
}

void NetworkChangeNotifier::RemoveObserver(Observer* observer) {
  if (g_network_change_notifier)
    g_network_change_notifier->observer_list_->RemoveObserver(observer);
}

NetworkChangeNotifier::NetworkChangeNotifier()
    : observer_list_(
        new ObserverListThreadSafe<Observer>(
            ObserverListBase<Observer>::NOTIFY_EXISTING_ONLY)) {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
}

void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->observer_list_->Notify(
        &Observer::OnIPAddressChanged);
  }
}

}  // namespace net
