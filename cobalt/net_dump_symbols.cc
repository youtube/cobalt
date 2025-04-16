// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// Give the linker all net symbols it needs to build cobalt.

#include "net/base/network_change_notifier_passive.h"
#include "net/proxy_resolution/proxy_config_service_linux.h"

// --- Add necessary includes ---
#include "base/notreached.h"     // Needed for NOTIMPLEMENTED()
#include "build/build_config.h"  // Needed for BUILDFLAG(IS_LINUX)
// --- End includes ---

namespace net {

// --- Stub implementations for NetworkChangeNotifierPassive ---

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype) {}

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier) {}

NetworkChangeNotifierPassive::~NetworkChangeNotifierPassive() {
  NOTIMPLEMENTED();
}

// Public Methods from Header
void NetworkChangeNotifierPassive::OnDNSChanged() {
  // Stub implementation
  NOTIMPLEMENTED();
}

void NetworkChangeNotifierPassive::OnIPAddressChanged() {
  // Stub implementation
  NOTIMPLEMENTED();
}

void NetworkChangeNotifierPassive::OnConnectionChanged(
    ConnectionType connection_type) {
  // Stub implementation
  NOTIMPLEMENTED();
}

void NetworkChangeNotifierPassive::OnConnectionSubtypeChanged(
    ConnectionType connection_type,
    ConnectionSubtype connection_subtype) {
  // Stub implementation
  NOTIMPLEMENTED();
}

// Protected Methods (Stubbing just in case they are called via base class ptr)
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierPassive::GetCurrentConnectionType() const {
  // Stub implementation
  NOTIMPLEMENTED();
  return NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

void NetworkChangeNotifierPassive::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  // Stub implementation
  NOTIMPLEMENTED();
}

void ProxyConfigServiceLinux::Delegate::SetUpAndFetchInitialConfig(
    const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const NetworkTrafficAnnotationTag& traffic_annotation) {}

#if BUILDFLAG(IS_LINUX)

// AddressMapCacheLinux::AddressMapCacheLinux() = default;
// AddressMapCacheLinux::~AddressMapCacheLinux() = default;
#endif  // BUILDFLAG(IS_LINUX)

// AddressMapOwnerLinux*
// NetworkChangeNotifierPassive::GetAddressMapOwnerInternal() {
//   // Stub implementation
//   NOTIMPLEMENTED();
//   return nullptr;
// }

// --- End stub implementations ---

}  // namespace net
