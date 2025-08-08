// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/window_properties.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/env.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

constexpr int kUnsupportedAppsViewSpacing = 8;

std::u16string GetStringWithQuotes(const std::u16string& str) {
  return u"\"" + str + u"\"";
}

}  // namespace

// The client view of the dialog. Contains a label which is a description, and
// optionally a couple images of unsupported apps.
class SavedDeskDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(SavedDeskDialog);

  SavedDeskDialog() {
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

    auto* layout_provider = views::LayoutProvider::Get();
    set_fixed_width(layout_provider->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    // Add the description for the dialog.
    AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&description_label_)
            .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL))
            .SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary))
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build());
  }
  SavedDeskDialog(const SavedDeskDialog&) = delete;
  SavedDeskDialog& operator=(const SavedDeskDialog&) = delete;
  ~SavedDeskDialog() override = default;

  void SetTitleText(int message_id) {
    SetTitle(l10n_util::GetStringUTF16(message_id));
  }

  void SetConfirmButtonText(int message_id) {
    SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(message_id));
  }

  void SetDescriptionText(const std::u16string& text) {
    DCHECK(description_label_);
    description_label_->SetText(text);
  }

  void SetDescriptionAccessibleName(const std::u16string& accessible_name) {
    DCHECK(description_label_);
    description_label_->SetAccessibleName(accessible_name);
  }

 private:
  raw_ptr<views::Label, ExperimentalAsh> description_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskDialog, views::DialogDelegateView)
VIEW_BUILDER_PROPERTY(int, TitleText)
VIEW_BUILDER_PROPERTY(int, ConfirmButtonText)
VIEW_BUILDER_PROPERTY(std::u16string, DescriptionText)
VIEW_BUILDER_PROPERTY(std::u16string, DescriptionAccessibleName)
END_VIEW_BUILDER

BEGIN_METADATA(SavedDeskDialog, views::DialogDelegateView)
END_METADATA

}  // namespace ash

// Must be in global namespace and defined before usage.
DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskDialog)

namespace ash {

//-----------------------------------------------------------------------------
// SavedDeskDialogController:

SavedDeskDialogController::SavedDeskDialogController() = default;

SavedDeskDialogController::~SavedDeskDialogController() {
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    dialog_widget_->CloseNow();
  }
}

void SavedDeskDialogController::ShowUnsupportedAppsDialog(
    aura::Window* root_window,
    const std::vector<aura::Window*>& unsupported_apps,
    size_t incognito_window_count,
    DesksController::GetDeskTemplateCallback callback,
    std::unique_ptr<DeskTemplate> desk_template) {
  // We can only bind `callback` once but the user has three possible paths
  // (accept, cancel, or close) so store `callback` and `desk_template` for
  // future usage once we know the user's decision.
  unsupported_apps_callback_ = std::move(callback);
  unsupported_apps_template_ = std::move(desk_template);

  // Note that this assumed unsupported apps which are not incognito browsers
  // are linux apps.
  std::u16string app_description;
  int app_description_id;
  if (incognito_window_count == 0) {
    app_description_id =
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_DIALOG_DESCRIPTION;
  } else if (incognito_window_count != unsupported_apps.size()) {
    app_description_id =
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_AND_INCOGNITO_DIALOG_DESCRIPTION;
  } else {
    app_description_id =
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_INCOGNITO_DIALOG_DESCRIPTION;
  }

  if (chromeos::features::IsJellyEnabled()) {
    auto unsupported_apps_view = std::make_unique<views::BoxLayoutView>();
    unsupported_apps_view->SetOrientation(
        views::BoxLayout::Orientation::kVertical);
    unsupported_apps_view->SetBetweenChildSpacing(kUnsupportedAppsViewSpacing);

    auto label_view = std::make_unique<views::Label>();
    label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label_view);
    label_view->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    label_view->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_HEADER));
    unsupported_apps_view->AddChildView(std::move(label_view));

    auto icon_container_view = std::make_unique<SavedDeskIconContainer>();
    icon_container_view->PopulateIconContainerFromWindows(unsupported_apps);
    unsupported_apps_view->AddChildView(std::move(icon_container_view));
    auto dialog =
        views::Builder<SystemDialogDelegateView>()
            .SetTitleText(l10n_util::GetStringUTF16(
                unsupported_apps_template_->type() ==
                        DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_IN_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_IN_DESK_DIALOG_TITLE))
            .SetAcceptButtonText(l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_CONFIRM_BUTTON))
            .SetDescription(l10n_util::GetStringUTF16(app_description_id))
            .SetCancelCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserCanceledUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .SetCloseCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserCanceledUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .SetAcceptCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserAcceptedUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .Build();
    dialog->SetMiddleContentView(std::move(unsupported_apps_view));
    dialog->SetMiddleContentAlignment(views::LayoutAlignment::kStart);
    CreateDialogWidget(std::move(dialog), root_window);
  } else {
    auto dialog =
        views::Builder<SavedDeskDialog>()
            .SetTitleText(
                unsupported_apps_template_->type() ==
                        DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_IN_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_IN_DESK_DIALOG_TITLE)
            .SetConfirmButtonText(
                IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_CONFIRM_BUTTON)
            .SetDescriptionText(l10n_util::GetStringUTF16(app_description_id))
            .SetCancelCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserCanceledUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .SetCloseCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserCanceledUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .SetAcceptCallback(base::BindOnce(
                &SavedDeskDialogController::OnUserAcceptedUnsupportedAppsDialog,
                weak_ptr_factory_.GetWeakPtr()))
            .AddChildren(
                views::Builder<views::Label>()
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                               14, gfx::Font::Weight::MEDIUM))
                    .SetEnabledColor(
                        AshColorProvider::Get()->GetContentLayerColor(
                            AshColorProvider::ContentLayerType::
                                kTextColorPrimary))
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_HEADER)),
                views::Builder<SavedDeskIconContainer>()
                    .PopulateIconContainerFromWindows(unsupported_apps))
            .Build();
    CreateDialogWidget(std::move(dialog), root_window);
  }
  RecordUnsupportedAppDialogShowHistogram(unsupported_apps_template_->type());
}

void SavedDeskDialogController::ShowReplaceDialog(
    aura::Window* root_window,
    const std::u16string& template_name,
    DeskTemplateType template_type,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback) {
  if (!CanShowDialog()) {
    return;
  }
  if (chromeos::features::IsJellyEnabled()) {
    auto dialog =
        views::Builder<SystemDialogDelegateView>()
            .SetTitleText(l10n_util::GetStringUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_TITLE))
            .SetAcceptButtonText(l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_CONFIRM_BUTTON))
            .SetDescription(l10n_util::GetStringFUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_DESCRIPTION
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_DESCRIPTION,
                GetStringWithQuotes(template_name)))
            .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_DESCRIPTION
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_DESCRIPTION,
                template_name))
            .SetAcceptCallback(std::move(on_accept_callback))
            .SetCancelCallback(std::move(on_cancel_callback))
            .Build();
    CreateDialogWidget(std::move(dialog), root_window);
  } else {
    auto dialog =
        views::Builder<SavedDeskDialog>()
            .SetTitleText(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_TITLE)
            .SetConfirmButtonText(
                IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_CONFIRM_BUTTON)
            .SetDescriptionText(l10n_util::GetStringFUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_DESCRIPTION
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_DESCRIPTION,
                GetStringWithQuotes(template_name)))
            .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_REPLACE_TEMPLATE_DIALOG_DESCRIPTION
                    : IDS_ASH_DESKS_TEMPLATES_REPLACE_DESK_DIALOG_DESCRIPTION,
                template_name))
            .SetAcceptCallback(std::move(on_accept_callback))
            .SetCancelCallback(std::move(on_cancel_callback))
            .Build();
    CreateDialogWidget(std::move(dialog), root_window);
  }
}

void SavedDeskDialogController::ShowDeleteDialog(
    aura::Window* root_window,
    const std::u16string& template_name,
    DeskTemplateType template_type,
    base::OnceClosure on_accept_callback) {
  if (!CanShowDialog()) {
    return;
  }
  if (chromeos::features::IsJellyEnabled()) {
    auto dialog =
        views::Builder<SystemDialogDelegateView>()
            .SetTitleText(l10n_util::GetStringUTF16(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_DELETE_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_DELETE_DESK_DIALOG_TITLE))
            .SetDescription(l10n_util::GetStringFUTF16(
                IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
                GetStringWithQuotes(template_name)))
            .SetAcceptButtonText(l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON))
            .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
                IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
                template_name))
            .SetAcceptCallback(std::move(on_accept_callback))
            .Build();
    CreateDialogWidget(std::move(dialog), root_window);
  } else {
    auto dialog =
        views::Builder<SavedDeskDialog>()
            .SetTitleText(
                template_type == DeskTemplateType::kTemplate
                    ? IDS_ASH_DESKS_TEMPLATES_DELETE_TEMPLATE_DIALOG_TITLE
                    : IDS_ASH_DESKS_TEMPLATES_DELETE_DESK_DIALOG_TITLE)
            .SetButtonLabel(
                ui::DIALOG_BUTTON_OK,
                l10n_util::GetStringUTF16(
                    IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON))
            .SetDescriptionText(l10n_util::GetStringFUTF16(
                IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
                GetStringWithQuotes(template_name)))
            .SetDescriptionAccessibleName(l10n_util::GetStringFUTF16(
                IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
                template_name))
            .SetAcceptCallback(std::move(on_accept_callback))
            .Build();
    CreateDialogWidget(std::move(dialog), root_window);
  }
}

void SavedDeskDialogController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  for (auto& overview_grid :
       Shell::Get()->overview_controller()->overview_session()->grid_list()) {
    if (auto* library_view = overview_grid->GetSavedDeskLibraryView()) {
      for (auto* templates_grid_view : library_view->grid_views()) {
        for (SavedDeskItemView* saved_desk_item :
             templates_grid_view->grid_items()) {
          // Update the button visibility when a dialog is closed.
          saved_desk_item->UpdateHoverButtonsVisibility(
              aura::Env::GetInstance()->last_mouse_location(),
              /*is_touch=*/false);
        }
      }
    }
  }
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
}

void SavedDeskDialogController::CreateDialogWidget(
    std::unique_ptr<views::WidgetDelegate> dialog,
    aura::Window* root_window) {
  // We should not get here with an active dialog.
  DCHECK_EQ(dialog_widget_, nullptr);

  // The dialog will show on the display associated with `root_window`, and will
  // block all input since it is system modal.
  DCHECK(root_window->IsRootWindow());
  dialog->SetModalType(ui::MODAL_TYPE_SYSTEM);
  dialog->SetShowCloseButton(false);

  if (chromeos::features::IsJellyEnabled()) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
    params.context = root_window;
    params.init_properties_container.SetProperty(kOverviewUiKey, true);
    params.delegate = dialog.release();

    dialog_widget_ = new views::Widget();
    dialog_widget_->Init(std::move(params));
  } else {
    dialog_widget_ = views::DialogDelegate::CreateDialogWidget(
        std::move(dialog),
        /*context=*/root_window, /*parent=*/nullptr);
    dialog_widget_->GetNativeWindow()->SetProperty(kOverviewUiKey, true);
  }
  dialog_widget_->GetNativeWindow()->SetName("TemplateDialogForTesting");
  dialog_widget_->Show();
  dialog_widget_observation_.Observe(dialog_widget_.get());

  // Ensure that if ChromeVox is enabled, it focuses on the dialog.
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (accessibility_controller->spoken_feedback().enabled()) {
    accessibility_controller->SetA11yOverrideWindow(
        dialog_widget_->GetNativeWindow());
  }
}

const SystemDialogDelegateView*
SavedDeskDialogController::GetSystemDialogViewForTesting() const {
  CHECK(chromeos::features::IsJellyEnabled());
  CHECK(dialog_widget_);
  return static_cast<SystemDialogDelegateView*>(
      dialog_widget_->GetContentsView());
}

bool SavedDeskDialogController::CanShowDialog() const {
  // Cannot show a dialog while another is active.
  return dialog_widget_ == nullptr;
}

void SavedDeskDialogController::OnUserAcceptedUnsupportedAppsDialog() {
  DCHECK(!unsupported_apps_callback_.is_null());
  DCHECK(unsupported_apps_template_);
  std::move(unsupported_apps_callback_)
      .Run(std::move(unsupported_apps_template_));
}

void SavedDeskDialogController::OnUserCanceledUnsupportedAppsDialog() {
  DCHECK(!unsupported_apps_callback_.is_null());

  // Make sure the saved desk buttons are enabled since the user should be able
  // to click them again after cancelling the unsupported apps dialog.
  for (auto& overview_grid :
       Shell::Get()->overview_controller()->overview_session()->grid_list()) {
    overview_grid->EnableSaveDeskButtonContainer();
  }

  std::move(unsupported_apps_callback_).Run(nullptr);
  unsupported_apps_template_.reset();
}

}  // namespace ash
