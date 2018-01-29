// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"

#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/dns/dns_config_service.h"

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
  virtual ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_UNKNOWN;
  }
};

}  // namespace

// The main observer class that records UMAs for network events.
class HistogramWatcher
    : public NetworkChangeNotifier::ConnectionTypeObserver,
      public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::DNSObserver,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  HistogramWatcher()
      : last_ip_address_change_(base::TimeTicks::Now()),
        last_connection_change_(base::TimeTicks::Now()),
        last_dns_change_(base::TimeTicks::Now()),
        last_network_change_(base::TimeTicks::Now()),
        last_connection_type_(NetworkChangeNotifier::CONNECTION_UNKNOWN),
        offline_packets_received_(0) {}

  // Registers our three Observer implementations.  This is called from the
  // network thread so that our Observer implementations are also called
  // from the network thread.  This avoids multi-threaded race conditions
  // because the only other interface, |NotifyDataReceived| is also
  // only called from the network thread.
  void Init() {
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
    NetworkChangeNotifier::AddIPAddressObserver(this);
    NetworkChangeNotifier::AddDNSObserver(this);
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  virtual ~HistogramWatcher() {}

  // NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() override {
    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.IPAddressChange",
                               SinceLast(&last_ip_address_change_));
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NCN.ConnectionTypeChangeToIPAddressChange",
        last_ip_address_change_ - last_connection_change_);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override {
    if (type != NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OnlineChange",
                                 SinceLast(&last_connection_change_));

      if (offline_packets_received_) {
        if ((last_connection_change_ - last_offline_packet_received_) <
            base::TimeDelta::FromSeconds(5)) {
          // We can compare this sum with the sum of NCN.OfflineDataRecv.
          UMA_HISTOGRAM_COUNTS_10000(
              "NCN.OfflineDataRecvAny5sBeforeOnline",
              offline_packets_received_);
        }

        UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineDataRecvUntilOnline",
                                   last_connection_change_ -
                                       last_offline_packet_received_);
      }
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineChange",
                                 SinceLast(&last_connection_change_));
    }
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NCN.IPAddressChangeToConnectionTypeChange",
        last_connection_change_ - last_ip_address_change_);

    offline_packets_received_ = 0;
    last_connection_type_ = type;
    polling_interval_ = base::TimeDelta::FromSeconds(1);
  }

  // NetworkChangeNotifier::DNSObserver implementation.
  virtual void OnDNSChanged() override {
    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.DNSConfigChange",
                               SinceLast(&last_dns_change_));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      NetworkChangeNotifier::ConnectionType type) override {
    if (type != NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.NetworkOnlineChange",
                                 SinceLast(&last_network_change_));
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.NetworkOfflineChange",
                                 SinceLast(&last_network_change_));
    }
  }

  // Record histogram data whenever we receive a packet but think we're
  // offline.  Should only be called from the network thread.
  void NotifyDataReceived(const GURL& source) {
    if (last_connection_type_ != NetworkChangeNotifier::CONNECTION_NONE ||
        IsLocalhost(source.host()) ||
        !(source.SchemeIs("http") || source.SchemeIs("https"))) {
      return;
    }

    base::TimeTicks current_time = base::TimeTicks::Now();
    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineDataRecv",
                               current_time - last_connection_change_);
    offline_packets_received_++;
    last_offline_packet_received_ = current_time;

    if ((current_time - last_polled_connection_) > polling_interval_) {
      polling_interval_ *= 2;
      last_polled_connection_ = current_time;
      base::TimeTicks started_get_connection_type = base::TimeTicks::Now();
      last_polled_connection_type_ =
          NetworkChangeNotifier::GetConnectionType();
      UMA_HISTOGRAM_TIMES("NCN.GetConnectionTypeTime",
                          base::TimeTicks::Now() -
                          started_get_connection_type);
    }
    if (last_polled_connection_type_ ==
        NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.PollingOfflineDataRecv",
                                 current_time - last_connection_change_);
    }
  }

 private:
  static base::TimeDelta SinceLast(base::TimeTicks *last_time) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeDelta delta = current_time - *last_time;
    *last_time = current_time;
    return delta;
  }

  base::TimeTicks last_ip_address_change_;
  base::TimeTicks last_connection_change_;
  base::TimeTicks last_dns_change_;
  base::TimeTicks last_network_change_;
  base::TimeTicks last_offline_packet_received_;
  base::TimeTicks last_polled_connection_;
  // |polling_interval_| is initialized by |OnConnectionTypeChanged| on our
  // first transition to offline and on subsequent transitions.  Once offline,
  // |polling_interval_| doubles as offline data is received and we poll
  // with |NetworkChangeNotifier::GetConnectionType| to verify the connection
  // state.
  base::TimeDelta polling_interval_;
  // |last_connection_type_| is the last value passed to
  // |OnConnectionTypeChanged|.
  NetworkChangeNotifier::ConnectionType last_connection_type_;
  // |last_polled_connection_type_| is last result from calling
  // |NetworkChangeNotifier::GetConnectionType| in |NotifyDataReceived|.
  NetworkChangeNotifier::ConnectionType last_polled_connection_type_;
  int32 offline_packets_received_;

  DISALLOW_COPY_AND_ASSIGN(HistogramWatcher);
};

// NetworkState is thread safe.
class NetworkChangeNotifier::NetworkState {
 public:
  NetworkState() {}
  ~NetworkState() {}

  void GetDnsConfig(DnsConfig* config) const {
    base::AutoLock lock(lock_);
    *config = dns_config_;
  }

  void SetDnsConfig(const DnsConfig& dns_config) {
    base::AutoLock lock(lock_);
    dns_config_ = dns_config;
  }

 private:
  mutable base::Lock lock_;
  DnsConfig dns_config_;
};

NetworkChangeNotifier::NetworkChangeCalculatorParams::
    NetworkChangeCalculatorParams() {
}

// Calculates NetworkChange signal from IPAddress and ConnectionType signals.
class NetworkChangeNotifier::NetworkChangeCalculator
    : public ConnectionTypeObserver,
      public IPAddressObserver {
 public:
  NetworkChangeCalculator(const NetworkChangeCalculatorParams& params)
      : params_(params),
        have_announced_(false),
        last_announced_connection_type_(CONNECTION_NONE),
        pending_connection_type_(CONNECTION_NONE) {}

  void Init() {
    AddConnectionTypeObserver(this);
    AddIPAddressObserver(this);
  }

  virtual ~NetworkChangeCalculator() {
    RemoveConnectionTypeObserver(this);
    RemoveIPAddressObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() override {
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.ip_address_offline_delay_ : params_.ip_address_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(ConnectionType type) override {
    pending_connection_type_ = type;
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.connection_type_offline_delay_
        : params_.connection_type_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

 private:
  void Notify() {
    // Don't bother signaling about dead connections.
    if (have_announced_ &&
        (last_announced_connection_type_ == CONNECTION_NONE) &&
        (pending_connection_type_ == CONNECTION_NONE)) {
      return;
    }
    have_announced_ = true;
    last_announced_connection_type_ = pending_connection_type_;
    // Immediately before sending out an online signal, send out an offline
    // signal to perform any destructive actions before constructive actions.
    if (pending_connection_type_ != CONNECTION_NONE)
      NetworkChangeNotifier::NotifyObserversOfNetworkChange(CONNECTION_NONE);
    NetworkChangeNotifier::NotifyObserversOfNetworkChange(
        pending_connection_type_);
  }

  const NetworkChangeCalculatorParams params_;

  // Indicates if NotifyObserversOfNetworkChange has been called yet.
  bool have_announced_;
  // Last value passed to NotifyObserversOfNetworkChange.
  ConnectionType last_announced_connection_type_;
  // Value to pass to NotifyObserversOfNetworkChange when Notify is called.
  ConnectionType pending_connection_type_;
  // Used to delay notifications so duplicates can be combined.
  base::OneShotTimer<NetworkChangeCalculator> timer_;
};

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
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::GetConnectionType() {
  return g_network_change_notifier ?
      g_network_change_notifier->GetCurrentConnectionType() :
      CONNECTION_UNKNOWN;
}

// static
void NetworkChangeNotifier::GetDnsConfig(DnsConfig* config) {
  if (!g_network_change_notifier) {
    *config = DnsConfig();
  } else {
    g_network_change_notifier->network_state_->GetDnsConfig(config);
  }
}

// static
const char* NetworkChangeNotifier::ConnectionTypeToString(
    ConnectionType type) {
  static const char* kConnectionTypeNames[] = {
    "CONNECTION_UNKNOWN",
    "CONNECTION_ETHERNET",
    "CONNECTION_WIFI",
    "CONNECTION_2G",
    "CONNECTION_3G",
    "CONNECTION_4G",
    "CONNECTION_NONE"
  };
  COMPILE_ASSERT(
      arraysize(kConnectionTypeNames) ==
          NetworkChangeNotifier::CONNECTION_NONE + 1,
      ConnectionType_name_count_mismatch);
  if (type < CONNECTION_UNKNOWN || type > CONNECTION_NONE) {
    NOTREACHED();
    return "CONNECTION_INVALID";
  }
  return kConnectionTypeNames[type];
}

// static
void NetworkChangeNotifier::NotifyDataReceived(const GURL& source) {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->histogram_watcher_->NotifyDataReceived(source);
}

// static
void NetworkChangeNotifier::InitHistogramWatcher() {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->histogram_watcher_->Init();
}

#if defined(OS_LINUX)
// static
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTracker() {
  return g_network_change_notifier ?
        g_network_change_notifier->GetAddressTrackerInternal() : NULL;
}
#endif

// static
bool NetworkChangeNotifier::IsOffline() {
   return GetConnectionType() == CONNECTION_NONE;
}

// static
bool NetworkChangeNotifier::IsConnectionCellular(ConnectionType type) {
  bool is_cellular = false;
  switch (type) {
    case CONNECTION_2G:
    case CONNECTION_3G:
    case CONNECTION_4G:
      is_cellular =  true;
      break;
    case CONNECTION_UNKNOWN:
    case CONNECTION_ETHERNET:
    case CONNECTION_WIFI:
    case CONNECTION_NONE:
      is_cellular = false;
      break;
  }
  return is_cellular;
}

// static
NetworkChangeNotifier* NetworkChangeNotifier::CreateMock() {
  return new MockNetworkChangeNotifier();
}

void NetworkChangeNotifier::AddIPAddressObserver(IPAddressObserver* observer) {
  if (g_network_change_notifier)
    g_network_change_notifier->ip_address_observer_list_->AddObserver(observer);
}

void NetworkChangeNotifier::AddConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::AddDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::AddNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->AddObserver(
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

void NetworkChangeNotifier::RemoveConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->RemoveObserver(
        observer);
  }
}

NetworkChangeNotifier::NetworkChangeNotifier(
    const NetworkChangeCalculatorParams& params
        /*= NetworkChangeCalculatorParams()*/)
    : ip_address_observer_list_(
        new ObserverListThreadSafe<IPAddressObserver>(
            ObserverListBase<IPAddressObserver>::NOTIFY_EXISTING_ONLY)),
      connection_type_observer_list_(
        new ObserverListThreadSafe<ConnectionTypeObserver>(
            ObserverListBase<ConnectionTypeObserver>::NOTIFY_EXISTING_ONLY)),
      resolver_state_observer_list_(
        new ObserverListThreadSafe<DNSObserver>(
            ObserverListBase<DNSObserver>::NOTIFY_EXISTING_ONLY)),
      network_change_observer_list_(
        new ObserverListThreadSafe<NetworkChangeObserver>(
            ObserverListBase<NetworkChangeObserver>::NOTIFY_EXISTING_ONLY)),
      network_state_(new NetworkState()),
      histogram_watcher_(new HistogramWatcher()),
      network_change_calculator_(new NetworkChangeCalculator(params)) {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
  network_change_calculator_->Init();
}

#if defined(OS_LINUX)
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTrackerInternal() const {
  return NULL;
}
#endif

// static
void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->Notify(
        &IPAddressObserver::OnIPAddressChanged);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfDNSChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->Notify(
        &DNSObserver::OnDNSChanged);
  }
}

// static
void NetworkChangeNotifier::SetDnsConfig(const DnsConfig& config) {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->network_state_->SetDnsConfig(config);
  NotifyObserversOfDNSChange();
}

void NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->Notify(
        &ConnectionTypeObserver::OnConnectionTypeChanged,
        GetConnectionType());
  }
}

void NetworkChangeNotifier::NotifyObserversOfNetworkChange(
    ConnectionType type) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->Notify(
        &NetworkChangeObserver::OnNetworkChanged,
        type);
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
