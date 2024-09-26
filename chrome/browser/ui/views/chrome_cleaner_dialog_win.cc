// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_dialog_win.h"

#include <string>

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowChromeCleanerPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerDialogController* dialog_controller,
    safe_browsing::ChromeCleanerController* cleaner_controller) {
  ChromeCleanerDialog* dialog =
      new ChromeCleanerDialog(dialog_controller, cleaner_controller);
  dialog->Show(browser);
}

}  // namespace chrome

////////////////////////////////////////////////////////////////////////////////
// ChromeCleanerDialog

enum class ChromeCleanerDialog::DialogInteractionResult {
  kAccept,
  kCancel,
  kClose,
  kClosedWithoutUserInteraction
};

ChromeCleanerDialog::ChromeCleanerDialog(
    safe_browsing::ChromeCleanerDialogController* dialog_controller,
    safe_browsing::ChromeCleanerController* cleaner_controller)
    : dialog_controller_(dialog_controller),
      cleaner_controller_(cleaner_controller) {
  DCHECK(dialog_controller_);
  DCHECK(cleaner_controller_);

  SetTitle(IDS_CHROME_CLEANUP_PROMPT_TITLE);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_PROMPT_REMOVE_BUTTON_LABEL));
  details_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ChromeCleanerDialog::DetailsButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_CHROME_CLEANUP_PROMPT_DETAILS_BUTTON_LABEL)));

  SetAcceptCallback(
      base::BindOnce(&ChromeCleanerDialog::HandleDialogInteraction,
                     base::Unretained(this), DialogInteractionResult::kAccept));
  SetCancelCallback(
      base::BindOnce(&ChromeCleanerDialog::HandleDialogInteraction,
                     base::Unretained(this), DialogInteractionResult::kCancel));
  SetCloseCallback(base::BindOnce(&ChromeCleanerDialog::HandleDialogInteraction,
                                  base::Unretained(this),
                                  DialogInteractionResult::kClose));

  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  set_margins(layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_PROMPT_EXPLANATION));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  logs_permission_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_LOGS_PERMISSION),
      base::BindRepeating(&ChromeCleanerDialog::LogsPermissionCheckboxPressed,
                          base::Unretained(this)));
  logs_permission_checkbox_->SetChecked(dialog_controller_->LogsEnabled());
  if (dialog_controller_->LogsManaged())
    logs_permission_checkbox_->SetState(views::Checkbox::STATE_DISABLED);

  AddChildView(label);
  AddChildView(logs_permission_checkbox_.get());
}

ChromeCleanerDialog::~ChromeCleanerDialog() {
  // Removing ourselves as observer of cleaner_controller_ should be done first,
  // so that subsequent actions in the desctructor do not cause any observer
  // functions to be called.
  cleaner_controller_->RemoveObserver(this);

  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (dialog_controller_)
    dialog_controller_->Cancel();
}

void ChromeCleanerDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(!browser_);
  DCHECK(dialog_controller_);
  DCHECK(cleaner_controller_);
  DCHECK_EQ(safe_browsing::ChromeCleanerController::State::kInfected,
            cleaner_controller_->state());

  browser_ = browser;
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())
      ->Show();
  dialog_controller_->DialogShown();

  // Observe the ChromeCleanerController's state so that the dialog can be
  // closed if the controller leaves the kInfected state before the user
  // interacts with the dialog.
  cleaner_controller_->AddObserver(this);
}

views::View* ChromeCleanerDialog::GetInitiallyFocusedView() {
  // Set focus away from the Remove/OK button to prevent accidental prompt
  // acceptance if the user is typing as the dialog appears.
  DCHECK(details_button_);
  return details_button_;
}

// safe_browsing::ChromeCleanerController::Observer overrides

void ChromeCleanerDialog::OnIdle(
    safe_browsing::ChromeCleanerController::IdleReason idle_reason) {
  Abort();
}

void ChromeCleanerDialog::OnScanning() {
  Abort();
}

void ChromeCleanerDialog::OnCleaning(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  Abort();
}

void ChromeCleanerDialog::OnRebootRequired() {
  Abort();
}

void ChromeCleanerDialog::HandleDialogInteraction(
    DialogInteractionResult result) {
  // This call must happen before the dialog controller is notified, otherwise
  // we might receive further notifications about the cleaner controller's state
  // changes.
  cleaner_controller_->RemoveObserver(this);

  if (!dialog_controller_)
    return;

  switch (result) {
    case DialogInteractionResult::kAccept:
      dialog_controller_->Accept(
          /*logs_enabled=*/logs_permission_checkbox_->GetChecked());
      break;
    case DialogInteractionResult::kCancel:
      dialog_controller_->Cancel();
      break;
    case DialogInteractionResult::kClose:
      dialog_controller_->Close();
      break;
    case DialogInteractionResult::kClosedWithoutUserInteraction:
      dialog_controller_->ClosedWithoutUserInteraction();
      break;
  }
  dialog_controller_ = nullptr;
}

void ChromeCleanerDialog::Abort() {
  HandleDialogInteraction(
      DialogInteractionResult::kClosedWithoutUserInteraction);
  GetWidget()->Close();
}

void ChromeCleanerDialog::DetailsButtonPressed() {
  if (dialog_controller_) {
    dialog_controller_->DetailsButtonClicked(
        /*logs_enabled=*/logs_permission_checkbox_->GetChecked());
    dialog_controller_ = nullptr;
  }
  GetWidget()->Close();
}

void ChromeCleanerDialog::LogsPermissionCheckboxPressed() {
  if (dialog_controller_)
    dialog_controller_->SetLogsEnabled(logs_permission_checkbox_->GetChecked());
}

BEGIN_METADATA(ChromeCleanerDialog, views::DialogDelegateView)
END_METADATA
