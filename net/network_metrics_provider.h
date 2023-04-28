// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/net/wifi_access_point_info_provider.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace net {
class NetworkQualityEstimator;
}

namespace metrics {

SystemProfileProto::Network::EffectiveConnectionType
ConvertEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type);

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class NetworkMetricsProvider
    : public MetricsProvider,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Class that provides |this| with the network quality estimator.
  class NetworkQualityEstimatorProvider {
   public:
    virtual ~NetworkQualityEstimatorProvider() {}

    // Returns the network quality estimator by calling |io_callback|. The
    // returned network quality estimator may be nullptr. |io_callback| must be
    // called on the IO thread. |io_callback| can be destroyed on IO thread only
    // after |this| is destroyed.
    virtual void PostReplyNetworkQualityEstimator(
        base::Callback<void(net::NetworkQualityEstimator*)> io_callback) = 0;

    // Returns the task runner on which |this| should be used and destroyed.
    virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() = 0;

   protected:
    NetworkQualityEstimatorProvider() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimatorProvider);
  };

  // Creates a NetworkMetricsProvider, where
  // |network_quality_estimator_provider| should be set if it is useful to
  // attach the quality of the network to the metrics report.
  explicit NetworkMetricsProvider(
      std::unique_ptr<NetworkQualityEstimatorProvider>
          network_quality_estimator_provider = nullptr);
  ~NetworkMetricsProvider() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest, EffectiveConnectionType);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ECTAmbiguousOnConnectionTypeChange);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ECTNotAmbiguousOnOffline);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ConnectionTypeIsAmbiguous);

  // Listens to the changes in the effective conection type.
  class EffectiveConnectionTypeObserver;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

  // NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  SystemProfileProto::Network::ConnectionType GetConnectionType() const;
  SystemProfileProto::Network::WifiPHYLayerProtocol GetWifiPHYLayerProtocol()
      const;

  // Posts a call to net::GetWifiPHYLayerProtocol on the blocking pool.
  void ProbeWifiPHYLayerProtocol();
  // Callback from the blocking pool with the result of
  // net::GetWifiPHYLayerProtocol.
  void OnWifiPHYLayerProtocolResult(net::WifiPHYLayerProtocol mode);

  // Writes info about the wireless access points that this system is
  // connected to.
  void WriteWifiAccessPointProto(
      const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
      SystemProfileProto::Network* network_proto);

  // Logs metrics that are functions of other metrics being uploaded.
  void LogAggregatedMetrics();

  // Notifies |this| that the effective connection type of the current network
  // has changed to |type|.
  void OnEffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to net::NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;
  // True if the network change notifier has been initialized.
  bool network_change_notifier_initialized_;

  // True if |wifi_phy_layer_protocol_| changed during the lifetime of the log.
  bool wifi_phy_layer_protocol_is_ambiguous_;
  // The PHY mode of the currently associated access point obtained via
  // net::GetWifiPHYLayerProtocol.
  net::WifiPHYLayerProtocol wifi_phy_layer_protocol_;

  // Helper object for retrieving connected wifi access point information.
  std::unique_ptr<WifiAccessPointInfoProvider> wifi_access_point_info_provider_;

  // These metrics track histogram totals for the Net.ErrorCodesForMainFrame4
  // histogram. They are used to compute deltas at upload time.
  base::HistogramBase::Count total_aborts_;
  base::HistogramBase::Count total_codes_;

  // Provides the network quality estimator. May be null.
  std::unique_ptr<NetworkQualityEstimatorProvider>
      network_quality_estimator_provider_;

  // Listens to the changes in the effective connection type. Initialized and
  // destroyed on the IO thread. May be null.
  std::unique_ptr<EffectiveConnectionTypeObserver>
      effective_connection_type_observer_;

  // Last known effective connection type.
  net::EffectiveConnectionType effective_connection_type_;

  // Minimum and maximum effective connection type since the metrics were last
  // provided.
  net::EffectiveConnectionType min_effective_connection_type_;
  net::EffectiveConnectionType max_effective_connection_type_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
