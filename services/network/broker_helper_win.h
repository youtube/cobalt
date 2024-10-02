// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BROKER_HELPER_WIN_H_
#define SERVICES_NETWORK_BROKER_HELPER_WIN_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"

namespace net {
class IPAddress;
}  // namespace net

namespace network {

// A class to create facilitate the creation of out-of-process sockets from
// within the Windows Sandbox. This is needed because the Windows App Container
// sandbox blocks network connections to services on the same host.
class COMPONENT_EXPORT(NETWORK_SERVICE) BrokerHelperWin
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  BrokerHelperWin();

  BrokerHelperWin(const BrokerHelperWin&) = delete;
  BrokerHelperWin& operator=(const BrokerHelperWin&) = delete;

  ~BrokerHelperWin() override;

  // Returns whether a connection to |address| would require the socket creation
  // to be brokered.
  bool ShouldBroker(const net::IPAddress& address) const;

 private:
  // NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void RefreshNetworkList();

  net::NetworkInterfaceList interfaces_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_BROKER_HELPER_WIN_H_
