// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bruschetta/bruschetta_uninstaller_view.h"
#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

BruschettaUninstallerView* g_bruschetta_uninstaller_view = nullptr;

}  // namespace

void BruschettaUninstallerView::Show(Profile* profile,
                                     const guest_os::GuestId& guest_id) {
  if (!g_bruschetta_uninstaller_view) {
    g_bruschetta_uninstaller_view =
        new BruschettaUninstallerView(profile, guest_id);
    views::DialogDelegate::CreateDialogWidget(g_bruschetta_uninstaller_view,
                                              nullptr, nullptr);
  }
  g_bruschetta_uninstaller_view->GetWidget()->Show();
}

bool BruschettaUninstallerView::Accept() {
  state_ = State::UNINSTALLING;
  SetButtons(ui::DIALOG_BUTTON_NONE);
  message_label_->SetText(l10n_util::GetStringUTF16(
      IDS_BRUSCHETTA_UNINSTALLER_UNINSTALLING_MESSAGE));

  progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
  // Setting value to -1 makes the progress bar play the
  // "indeterminate animation".
  progress_bar_->SetValue(-1);
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());

  bruschetta::BruschettaService::GetForProfile(profile_)->RemoveVm(
      guest_id_,
      base::BindOnce(&BruschettaUninstallerView::UninstallBruschettaFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  return false;  // Should not close the dialog
}

bool BruschettaUninstallerView::Cancel() {
  return true;  // Should close the dialog
}

// static
BruschettaUninstallerView*
BruschettaUninstallerView::GetActiveViewForTesting() {
  return g_bruschetta_uninstaller_view;
}

BruschettaUninstallerView::BruschettaUninstallerView(Profile* profile,
                                                     guest_os::GuestId guest_id)
    : profile_(profile), guest_id_(std::move(guest_id)) {
  SetShowCloseButton(false);
  SetTitle(IDS_BRUSCHETTA_UNINSTALLER_TITLE);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_UNINSTALLER_UNINSTALL_BUTTON));
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string device_type = ui::GetChromeOSDeviceName();
  const std::u16string message =
      l10n_util::GetStringFUTF16(IDS_BRUSCHETTA_UNINSTALLER_BODY, device_type);
  message_label_ = AddChildView(std::make_unique<views::Label>());
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

BruschettaUninstallerView::~BruschettaUninstallerView() {
  g_bruschetta_uninstaller_view = nullptr;
}

void BruschettaUninstallerView::HandleError(
    const std::u16string& error_message) {
  state_ = State::ERROR;
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  progress_bar_->SetVisible(false);
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void BruschettaUninstallerView::UninstallBruschettaFinished(bool success) {
  if (!success) {
    LOG(ERROR) << "Error uninstalling Bruschetta";
    HandleError(l10n_util::GetStringUTF16(IDS_BRUSCHETTA_UNINSTALLER_ERROR));
    return;
  }
  GetWidget()->Close();
}

BEGIN_METADATA(BruschettaUninstallerView, views::BubbleDialogDelegateView)
END_METADATA
