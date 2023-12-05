// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_linux.h"

#include <memory>

#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include <linux/ethtool.h>
#endif  // !BUILDFLAG(IS_ANDROID)
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <set>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces_posix.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/strings/string_piece.h"
#include "net/android/network_library.h"
#include "net/base/network_interfaces_getifaddrs.h"
#endif

namespace net {

namespace {

// When returning true, the platform native IPv6 address attributes were
// successfully converted to net IP address attributes. Otherwise, returning
// false and the caller should drop the IP address which can't be used by the
// application layer.
bool TryConvertNativeToNetIPAttributes(int native_attributes,
                                       int* net_attributes) {
  // For Linux/ChromeOS/Android, we disallow addresses with attributes
  // IFA_F_OPTIMISTIC, IFA_F_DADFAILED, and IFA_F_TENTATIVE as these
  // are still progressing through duplicated address detection (DAD)
  // and shouldn't be used by the application layer until DAD process
  // is completed.
  if (native_attributes & (
#if !BUILDFLAG(IS_ANDROID)
                              IFA_F_OPTIMISTIC | IFA_F_DADFAILED |
#endif  // !BUILDFLAG(IS_ANDROID)
                              IFA_F_TENTATIVE)) {
    return false;
  }

  if (native_attributes & IFA_F_TEMPORARY) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
  }

  if (native_attributes & IFA_F_DEPRECATED) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
  }

  return true;
}

}  // namespace

namespace internal {

// Gets the connection type for interface |ifname| by checking for wireless
// or ethtool extensions.
NetworkChangeNotifier::ConnectionType GetInterfaceConnectionType(
    const std::string& ifname) {
  return NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

std::string GetInterfaceSSID(const std::string& ifname) {
  return std::string();
}

bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const std::unordered_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name) {
  return true;
}

std::string GetWifiSSIDFromInterfaceListInternal(
    const NetworkInterfaceList& interfaces,
    internal::GetInterfaceSSIDFunction get_interface_ssid) {
  std::string connected_ssid;
  for (size_t i = 0; i < interfaces.size(); ++i) {
    if (interfaces[i].type != NetworkChangeNotifier::CONNECTION_WIFI)
      return std::string();
    std::string ssid = get_interface_ssid(interfaces[i].name);
    if (i == 0) {
      connected_ssid = ssid;
    } else if (ssid != connected_ssid) {
      return std::string();
    }
  }
  return connected_ssid;
}

base::ScopedFD GetSocketForIoctl() {
  return base::ScopedFD();
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  if (networks == nullptr)
    return false;

#if BUILDFLAG(IS_ANDROID)
  // On Android 11 RTM_GETLINK (used by AddressTrackerLinux) no longer works as
  // per https://developer.android.com/preview/privacy/mac-address so instead
  // use getifaddrs() which is supported since Android N.
  base::android::BuildInfo* build_info =
      base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() >= base::android::SDK_VERSION_NOUGAT) {
    // Some Samsung devices with MediaTek processors are with
    // a buggy getifaddrs() implementation,
    // so use a Chromium's own implementation to workaround.
    // See https://crbug.com/1240237 for more context.
    bool use_alternative_getifaddrs =
        base::StringPiece(build_info->brand()) == "samsung" &&
        base::StartsWith(build_info->hardware(), "mt");
    bool ret = internal::GetNetworkListUsingGetifaddrs(
        networks, policy, use_alternative_getifaddrs);
    // Use GetInterfaceConnectionType() to sharpen up interface types.
    for (NetworkInterface& network : *networks)
      network.type = internal::GetInterfaceConnectionType(network.name);
    return ret;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  const AddressMapOwnerLinux* map_owner = nullptr;
  // absl::optional<internal::AddressTrackerLinux> temp_tracker;
#if BUILDFLAG(IS_LINUX)
  // If NetworkChangeNotifier already maintains a map owner in this process, use
  // it.
  if (base::FeatureList::IsEnabled(features::kAddressTrackerLinuxIsProxied)) {
    map_owner = NetworkChangeNotifier::GetAddressMapOwner();
  }
#endif  // BUILDFLAG(IS_LINUX)
  if (!map_owner) {
    // If there is no existing map_owner, create an AdressTrackerLinux and
    // initialize it.
    // temp_tracker.emplace();
    // temp_tracker->Init();
    // map_owner = &temp_tracker.value();
  }

  return internal::GetNetworkListImpl(
      networks, policy, map_owner->GetOnlineLinks(), map_owner->GetAddressMap(),
      &internal::AddressTrackerLinux::GetInterfaceName);
}

std::string GetWifiSSID() {
// On Android, obtain the SSID using the Android-specific APIs.
#if BUILDFLAG(IS_ANDROID)
  return android::GetWifiSSID();
#else
  NetworkInterfaceList networks;
  if (GetNetworkList(&networks, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    return internal::GetWifiSSIDFromInterfaceListInternal(
        networks, internal::GetInterfaceSSID);
  }
  return std::string();
#endif
}

}  // namespace net
