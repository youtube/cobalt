// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_
#define ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_

#include "ash/ash_export.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This enum is tied directly to a UMA enum |NetworkRowClickedAction| defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class NetworkRowClickedAction {
  kConnectToNetwork = 0,
  kOpenNetworkSettingsPage = 1,
  kOpenSimUnlockDialog = 2,
  kOpenPortalSignin = 3,
  kMaxValue = kOpenPortalSignin
};

// This enum is tied directly to a UMA enum |DetailedViewSection| defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class DetailedViewSection {
  kWifiSection = 0,
  kMobileSection = 1,
  kEthernetSection = 2,
  kDetailedSection = 3,
  kMaxValue = kDetailedSection
};

ASH_EXPORT void RecordNetworkRowClickedAction(NetworkRowClickedAction action);

ASH_EXPORT void RecordDetailedViewSection(DetailedViewSection section);

ASH_EXPORT void RecordNetworkTypeToggled(
    chromeos::network_config::mojom::NetworkType network_type,
    bool new_state);

// Returns the subtext to display for a connected network in a portal state.
// This is used in the network menu, the tooltip, and for a11y.
ASH_EXPORT absl::optional<std::u16string> GetPortalStateSubtext(
    const chromeos::network_config::mojom::PortalState& portal_state);

// Returns true if current network row is disabled.
ASH_EXPORT bool IsNetworkDisabled(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties);

// Returns true if current network is a cellular network and is inhibited.
ASH_EXPORT bool IsNetworkInhibited(
    const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
        network_properties);

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_UTILS_H_
