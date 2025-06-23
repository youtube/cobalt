// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/starboard/linker_stub.h"
#include "net/base/network_change_notifier_passive.h"

namespace net {

// TODO: b/413130400 - Cobalt: Resolve all the linker stubs
// Note: This is a stub implementation. The actual implementation is in
// network_change_notifier_passive.cc which is not included in the cobalt build.
// Stub symbols have been provided here to satisfy the linker and build cobalt

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype) {
  COBALT_LINKER_STUB();
}

NetworkChangeNotifierPassive::NetworkChangeNotifierPassive(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier) {
  COBALT_LINKER_STUB();
}

NetworkChangeNotifierPassive::~NetworkChangeNotifierPassive() {
  COBALT_LINKER_STUB();
}

void NetworkChangeNotifierPassive::OnDNSChanged() {
  COBALT_LINKER_STUB();
}

void NetworkChangeNotifierPassive::OnIPAddressChanged() {
  COBALT_LINKER_STUB();
}

void NetworkChangeNotifierPassive::OnConnectionChanged(
    ConnectionType connection_type) {
  COBALT_LINKER_STUB();
}

void NetworkChangeNotifierPassive::OnConnectionSubtypeChanged(
    ConnectionType connection_type,
    ConnectionSubtype connection_subtype) {
  COBALT_LINKER_STUB();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierPassive::GetCurrentConnectionType() const {
  COBALT_LINKER_STUB();
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  return NetworkChangeNotifier::CONNECTION_UNKNOWN;
#endif
}

void NetworkChangeNotifierPassive::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  COBALT_LINKER_STUB();
}
}  // namespace net
