// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(base::BindRepeating(&BrowserAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      toolbar_view_(toolbar_view) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;

  UpdateIcon();
  UpdateTextAndHighlightColor();
}

void BrowserAppMenuButton::ShowMenu(int run_types) {
  if (IsMenuShowing())
    return;

#if BUILDFLAG(IS_CHROMEOS)
  if (auto* input_method = GetInputMethod()) {
    if (auto* controller = input_method->GetVirtualKeyboardController();
        controller && controller->IsKeyboardVisible()) {
      input_method->SetVirtualKeyboardVisibilityIfEnabled(false);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Browser* browser = toolbar_view_->browser();

  // If the menu was opened while reopen tab in-product help was
  // showing, we continue the IPH into the menu. Notify the promo
  // controller we are taking control of the promo.
  AlertMenuItem alert_item = CloseFeaturePromoAndContinue();

  RunMenu(std::make_unique<AppMenuModel>(
              toolbar_view_, browser, toolbar_view_->app_menu_icon_controller(),
              alert_item),
          browser, run_types);
}

AlertMenuItem BrowserAppMenuButton::CloseFeaturePromoAndContinue() {
  Browser* browser = toolbar_view_->browser();
  BrowserWindow* browser_window = browser->window();

  if (browser_window == nullptr)
    return AlertMenuItem::kNone;

  promo_handle_ = browser_window->CloseFeaturePromoAndContinue(
      feature_engagement::kIPHReopenTabFeature);

  if (promo_handle_.is_valid())
    return AlertMenuItem::kReopenTabs;

  promo_handle_ = browser_window->CloseFeaturePromoAndContinue(
      feature_engagement::kIPHHighEfficiencyModeFeature);

  if (promo_handle_.is_valid())
    return AlertMenuItem::kPerformance;

  return AlertMenuItem::kNone;
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateTextAndHighlightColor();
  AppMenuButton::OnThemeChanged();
}

void BrowserAppMenuButton::UpdateIcon() {
  const gfx::VectorIcon& icon =
      ui::TouchUiController::Get()->touch_ui()
          ? kBrowserToolsTouchIcon
          : (features::IsChromeRefresh2023() ? kBrowserToolsChromeRefreshIcon
                                             : kBrowserToolsIcon);
  for (auto state : kButtonStates) {
    SkColor icon_color =
        toolbar_view_->app_menu_icon_controller()->GetIconColor(
            GetForegroundColor(state));
    SetImageModel(state, ui::ImageModel::FromVectorIcon(icon, icon_color));
  }
}

void BrowserAppMenuButton::HandleMenuClosed() {
  // If we were showing a promo in the menu, drop the handle to notify
  // FeaturePromoController we're done. This is a no-op if we weren't
  // showing the promo.
  promo_handle_.Release();
}

void BrowserAppMenuButton::UpdateTextAndHighlightColor() {
  int tooltip_message_id;
  std::u16string text;
  if (type_and_severity_.severity == AppMenuIconController::Severity::NONE) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP;
  } else if (type_and_severity_.type ==
             AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    int message_id = IDS_APP_MENU_BUTTON_UPDATE;
    if (base::FeatureList::IsEnabled(features::kUpdateTextOptions)) {
      if (features::kUpdateTextOptionNumber.Get() == 1) {
        message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT1;
      } else if (features::kUpdateTextOptionNumber.Get() == 2) {
        message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT2;
      } else {
        message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT3;
      }
    }
    text = l10n_util::GetStringUTF16(message_id);
#else
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_UPDATE);
#endif
  } else {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_ALERT;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_ERROR);
  }

  absl::optional<SkColor> color;
  const auto* const color_provider = GetColorProvider();
  switch (type_and_severity_.severity) {
    case AppMenuIconController::Severity::NONE:
      break;
    case AppMenuIconController::Severity::LOW:
      color = color_provider->GetColor(kColorAppMenuHighlightSeverityLow);
      break;
    case AppMenuIconController::Severity::MEDIUM:
      color = color_provider->GetColor(kColorAppMenuHighlightSeverityMedium);
      break;
    case AppMenuIconController::Severity::HIGH:
      color = color_provider->GetColor(kColorAppMenuHighlightSeverityHigh);
      break;
  }

  SetTooltipText(l10n_util::GetStringUTF16(tooltip_message_id));
  SetHighlight(text, color);
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateColorsAndInsets();
  PreferredSizeChanged();
}

void BrowserAppMenuButton::ButtonPressed(const ui::Event& event) {
  ShowMenu(event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                              : views::MenuRunner::NO_FLAGS);
}

BEGIN_METADATA(BrowserAppMenuButton, AppMenuButton)
END_METADATA
