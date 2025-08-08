// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_wifi_header_view_impl.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

NetworkListWifiHeaderViewImpl::NetworkListWifiHeaderViewImpl(
    NetworkListNetworkHeaderView::Delegate* delegate)
    : NetworkListWifiHeaderView(delegate) {}

NetworkListWifiHeaderViewImpl::~NetworkListWifiHeaderViewImpl() = default;

void NetworkListWifiHeaderViewImpl::SetToggleState(bool enabled,
                                                   bool is_on,
                                                   bool animate_toggle) {
  std::u16string tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_WIFI,
      l10n_util::GetStringUTF16(
          is_on ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED));
  entry_row()->SetTooltipText(tooltip_text);
  toggle()->SetTooltipText(tooltip_text);
  NetworkListNetworkHeaderView::SetToggleState(enabled, is_on, animate_toggle);
}

void NetworkListWifiHeaderViewImpl::OnToggleToggled(bool is_on) {
  // Join wifi entry is not updated here, it will be updated when WiFi device
  // state changes.
  delegate()->OnWifiToggleClicked(is_on);
}

}  // namespace ash
