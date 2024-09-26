// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/separator.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class HoverHighlightView;
class NetworkDetailedNetworkView;

// Implementation of NetworkListViewController.
class ASH_EXPORT NetworkListViewControllerImpl
    : public TrayNetworkStateObserver,
      public NetworkListViewController,
      public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  NetworkListViewControllerImpl(
      NetworkDetailedNetworkView* network_detailed_network_view);
  NetworkListViewControllerImpl(const NetworkListViewController&) = delete;
  NetworkListViewControllerImpl& operator=(
      const NetworkListViewControllerImpl&) = delete;
  ~NetworkListViewControllerImpl() override;

 protected:
  TrayNetworkStateModel* model() { return model_; }

  NetworkDetailedNetworkView* network_detailed_network_view() {
    return network_detailed_network_view_;
  }

 private:
  friend class NetworkListViewControllerTest;
  friend class FakeNetworkDetailedNetworkView;

  // Used for testing. Starts at 11 to avoid collision with header view
  // child elements.
  enum class NetworkListViewControllerViewChildId {
    kConnectionWarning = 11,
    kConnectionWarningLabel = 12,
    kMobileSeparator = 13,
    kMobileStatusMessage = 14,
    kMobileSectionHeader = 15,
    kWifiSeparator = 16,
    kWifiSectionHeader = 17,
    kWifiStatusMessage = 18,
    kConnectionWarningSystemIcon = 19,
    kConnectionWarningManagedIcon = 20
  };

  // Map of network guids and their corresponding list item views.
  using NetworkIdToViewMap =
      base::flat_map<std::string, NetworkListNetworkItemView*>;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;
  void GlobalPolicyChanged() override;

  // bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  // Called to initialize views and when network list is recently updated.
  void GetNetworkStateList();
  void OnGetNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Checks `networks` and caches whether Mobile network, WiFi networks and vpn
  // networks exist in the list of `networks`. Also caches if a Mobile and
  // WiFi networks are enabled.
  void UpdateNetworkTypeExistence(
      const std::vector<
          chromeos::network_config::mojom::NetworkStatePropertiesPtr>&
          networks);

  // Adds a warning indicator if connected to a VPN, if the default network
  // has a proxy installed or if the secure DNS template URIs contain user or
  // device identifiers.
  size_t ShowConnectionWarningIfNetworkMonitored(size_t index);

  // Returns true if mobile data section should be added to view.
  bool ShouldMobileDataSectionBeShown();

  // Creates if missing and adds a Mobile or Wifi separator to the view.
  // Also reorders separator view in network list. A reference to the
  // separator is captured in `*separator_view`.
  size_t CreateSeparatorIfMissingAndReorder(size_t index,
                                            views::Separator** separator_view);

  // Creates the wifi group header for wifi networks. If `is_known` is `true`,
  // it creates the "Known networks" header, which is the `known_header_`. If
  // `is_known` is false, it creates "Unknown networks" header, which is the
  // `unknown_header_`.
  size_t CreateWifiGroupHeader(size_t index, const bool is_known);

  // Creates and adds the join wifi entry at the bottom of the wifi networks.
  size_t CreateJoinWifiEntry(size_t index);

  // Updates Mobile data section, updates add eSIM button states and
  // calls UpdateMobileToggleAndSetStatusMessage().
  void UpdateMobileSection();

  // Updates the WiFi data section. This method creates a new header if one does
  // not exist, and will update both the WiFi toggle and "add network" button.
  // If there are no WiFi networks or WiFi is disabled, this method will also
  // add an info message.
  void UpdateWifiSection();

  // Updated mobile data toggle states and sets mobile data status message.
  void UpdateMobileToggleAndSetStatusMessage();

  // Creates an info label if missing and updates info label message.
  void CreateInfoLabelIfMissingAndUpdate(int message_id,
                                         TrayInfoLabel** info_label_ptr);

  // Creates a NetworkListNetworkItem if it does not exist else uses the
  // existing view, also reorders it in NetworkDetailedNetworkView scroll list.
  size_t CreateItemViewsIfMissingAndReorder(
      chromeos::network_config::mojom::NetworkType type,
      size_t index,
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>&
          networks,
      NetworkIdToViewMap* previous_views);

  // Creates a view that indicates connections might be monitored if
  // connected to a VPN, if the default network has a proxy installed or if the
  // secure DNS template URIs contain identifiers.
  void ShowConnectionWarning(bool show_managed_icon);

  // Hides a connection warning, if visible.
  void HideConnectionWarning();

  // Determines whether a scan for WiFi and Tether networks should be requested
  // and updates the scanning bar accordingly.
  void UpdateScanningBarAndTimer();

  // Calls RequestScan() and starts a timer that will repeatedly call
  // RequestScan() after a delay.
  void ScanAndStartTimer();

  // Immediately request a WiFi and Tether network scan.
  void RequestScan();

  // Focuses on last selected view in NetworkDetailedNetworkView scroll list.
  void FocusLastSelectedView();

  // Sets an icon next to the connection warning text; if `use_managed_icon` is
  // true, the managed icon is shown, otherwise the system info icon. If an icon
  // already exists, it will be replaced.
  void SetConnectionWarningIcon(TriView* parent, bool use_managed_icon);

  // Called when the managed properties for the network identified by `guid` are
  // fetched.
  void OnGetManagedPropertiesResult(
      const std::string& guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr properties);

  // Checks if the network is managed and, if true, replaces the system icon
  // shown next to the privacy warning message with a managed icon. Only called
  // if the default network has a proxy configured or if a VPN is active.
  void MaybeShowConnectionWarningManagedIcon(bool using_proxy);

  raw_ptr<TrayNetworkStateModel, ExperimentalAsh> model_;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  TrayInfoLabel* mobile_status_message_ = nullptr;
  NetworkListMobileHeaderView* mobile_header_view_ = nullptr;
  views::Separator* mobile_separator_view_ = nullptr;
  TriView* connection_warning_ = nullptr;

  // Pointer to the icon displayed next to the connection warning message when
  // a proxy or a VPN is active. Owned by `connection_warning_`. If the network
  // is monitored by the admin, via policy, it displays the managed icon,
  // otherwise the system icon.
  views::ImageView* connection_warning_icon_ = nullptr;
  // Owned by `connection_warning_`.
  raw_ptr<views::Label, DanglingUntriaged | ExperimentalAsh>
      connection_warning_label_ = nullptr;

  raw_ptr<NetworkListWifiHeaderView, ExperimentalAsh> wifi_header_view_ =
      nullptr;
  views::Separator* wifi_separator_view_ = nullptr;
  TrayInfoLabel* wifi_status_message_ = nullptr;

  // Owned by views hierarchy.
  views::Label* known_header_ = nullptr;
  views::Label* unknown_header_ = nullptr;
  HoverHighlightView* join_wifi_entry_ = nullptr;

  bool has_mobile_networks_;
  bool has_wifi_networks_;
  bool is_mobile_network_enabled_;
  bool is_wifi_enabled_;
  std::string connected_vpn_guid_;

  // Can be nullopt while the managed properties of the network are being
  // fetched via mojo. If one of `is_proxy_managed_` or `is_vpn_managed_` is
  // true, the system icon shown next to the privacy warning is replaced by a
  // managed icon.
  absl::optional<bool> is_proxy_managed_;
  absl::optional<bool> is_vpn_managed_;

  raw_ptr<NetworkDetailedNetworkView, ExperimentalAsh>
      network_detailed_network_view_;
  NetworkIdToViewMap network_id_to_view_map_;

  // Timer for repeatedly requesting network scans with a delay between
  // requests.
  base::RepeatingTimer network_scan_repeating_timer_;

  base::WeakPtrFactory<NetworkListViewControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
