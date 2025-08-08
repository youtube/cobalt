// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_NETWORK_CHANGE_NOTIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_NETWORK_CHANGE_NOTIFIER_H_

#include "base/memory/raw_ptr.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"

namespace android_webview {

// AwNetworkChangeNotifier is similar to NetworkChangeNotifierAndroid except
// it only propagates max-bandwidth changes in order to make the Network
// Information API work in blink.
//
// This somewhat reduced functionality is necessary because of the way
// NetworkChangeNotifier is enabled in WebView. It is enabled only when there
// are living WebView instances (instead of using ApplicationStatus) hence the
// existing Chrome for Android implementation is not applicable as is
// (see crbug.com/529434).
class AwNetworkChangeNotifier
    : public net::NetworkChangeNotifier,
      public net::NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  AwNetworkChangeNotifier(const AwNetworkChangeNotifier&) = delete;
  AwNetworkChangeNotifier& operator=(const AwNetworkChangeNotifier&) = delete;

  ~AwNetworkChangeNotifier() override;

  // NetworkChangeNotifier:
  ConnectionType GetCurrentConnectionType() const override;
  // Requires ACCESS_WIFI_STATE permission in order to provide precise WiFi link
  // speed.
  void GetCurrentMaxBandwidthAndConnectionType(
      double* max_bandwidth_mbps,
      ConnectionType* connection_type) const override;
  bool AreNetworkHandlesCurrentlySupported() const override;
  void GetCurrentConnectedNetworks(NetworkList* network_list) const override;
  ConnectionType GetCurrentNetworkConnectionType(
      net::handles::NetworkHandle network) const override;
  net::handles::NetworkHandle GetCurrentDefaultNetwork() const override;

  // NetworkChangeNotifierDelegateAndroid::Observer:
  void OnConnectionTypeChanged() override;
  void OnConnectionCostChanged() override;
  void OnMaxBandwidthChanged(double max_bandwidth_mbps,
                             ConnectionType type) override;
  void OnNetworkConnected(net::handles::NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(net::handles::NetworkHandle network) override;
  void OnNetworkDisconnected(net::handles::NetworkHandle network) override;
  void OnNetworkMadeDefault(net::handles::NetworkHandle network) override;
  void OnDefaultNetworkActive() override;

 private:
  friend class AwNetworkChangeNotifierFactory;

  explicit AwNetworkChangeNotifier(
      net::NetworkChangeNotifierDelegateAndroid* delegate);

  static NetworkChangeCalculatorParams DefaultNetworkChangeCalculatorParams();

  const raw_ptr<net::NetworkChangeNotifierDelegateAndroid> delegate_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_NETWORK_CHANGE_NOTIFIER_H_
