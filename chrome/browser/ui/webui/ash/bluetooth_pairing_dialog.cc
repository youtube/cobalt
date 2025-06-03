// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"

#include <memory>

#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/bluetooth_shared_load_time_data_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/bluetooth_pairing_dialog_resources.h"
#include "chrome/grit/bluetooth_pairing_dialog_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/core/session_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

constexpr int kBluetoothPairingDialogHeight = 430;

void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"ok", IDS_OK},
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
  bluetooth::AddLoadTimeData(html_source);
}

}  // namespace

// static
SystemWebDialogDelegate* BluetoothPairingDialog::ShowDialog(
    absl::optional<base::StringPiece> device_address) {
  std::string dialog_id = chrome::kChromeUIBluetoothPairingURL;
  absl::optional<std::string> canonical_device_address;

  if (device_address.has_value()) {
    canonical_device_address =
        device::CanonicalizeBluetoothAddress(device_address.value());

    if (canonical_device_address->empty()) {
      LOG(ERROR) << "BluetoothPairingDialog: Invalid address: "
                 << device_address.value();
      return nullptr;
    }
    dialog_id = canonical_device_address.value();
  }

  SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(dialog_id);

  if (existing_dialog) {
    existing_dialog->Focus();
    return existing_dialog;
  }

  // This object manages its own lifetime so we don't bother wrapping in a
  // std::unique_ptr to release quickly after.
  BluetoothPairingDialog* dialog =
      new BluetoothPairingDialog(dialog_id, canonical_device_address);
  dialog->ShowSystemDialog();
  return dialog;
}

BluetoothPairingDialog::BluetoothPairingDialog(
    const std::string& dialog_id,
    absl::optional<base::StringPiece> canonical_device_address)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIBluetoothPairingURL),
                              /*title=*/std::u16string()),
      dialog_id_(dialog_id) {
  if (canonical_device_address.has_value())
    device_data_.Set("address", canonical_device_address.value());

  device_data_.Set("shouldOmitLinks",
                   session_manager::SessionManager::Get()->session_state() !=
                       session_manager::SessionState::ACTIVE);
}

BluetoothPairingDialog::~BluetoothPairingDialog() = default;

const std::string& BluetoothPairingDialog::Id() {
  return dialog_id_;
}

void BluetoothPairingDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

void BluetoothPairingDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth,
                kBluetoothPairingDialogHeight);
}

std::string BluetoothPairingDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(device_data_, &data);
  return data;
}

// BluetoothPairingUI

BluetoothPairingDialogUI::BluetoothPairingDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIBluetoothPairingHost);

  AddBluetoothStrings(source);
  source->AddLocalizedString("title", IDS_BLUETOOTH_PAIRING_PAIR_NEW_DEVICES);
  source->AddBoolean("isJellyEnabled", ::chromeos::features::IsJellyEnabled());

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kBluetoothPairingDialogResources,
                      kBluetoothPairingDialogResourcesSize),
      IDR_BLUETOOTH_PAIRING_DIALOG_BLUETOOTH_PAIRING_DIALOG_CONTAINER_HTML);
  // Enabling trusted types via trusted_types_util must be done after
  // webui::SetupWebUIDataSource to override the trusted type CSP with correct
  // policies for JS WebUIs.
  ash::EnableTrustedTypesCSP(source);

  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kStandalonePairingDialog);
}

BluetoothPairingDialogUI::~BluetoothPairingDialogUI() = default;

void BluetoothPairingDialogUI::BindInterface(
    mojo::PendingReceiver<bluetooth_config::mojom::CrosBluetoothConfig>
        receiver) {
  GetBluetoothConfigService(std::move(receiver));
}

void BluetoothPairingDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_change_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(BluetoothPairingDialogUI)

}  // namespace ash
