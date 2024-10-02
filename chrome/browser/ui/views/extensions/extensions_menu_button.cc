// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/style/typography.h"

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    ToolbarActionViewController* controller)
    : HoverButton(base::BindRepeating(&ExtensionsMenuButton::ButtonPressed,
                                      base::Unretained(this)),
                  std::u16string()),
      browser_(browser),
      controller_(controller) {
  controller_->SetDelegate(this);
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

void ExtensionsMenuButton::AddedToWidget() {
  ConfigureBubbleMenuItem(this, 0);
  UpdateState();
}

// ToolbarActionViewDelegateViews:
views::View* ExtensionsMenuButton::GetAsView() {
  return this;
}

views::FocusManager* ExtensionsMenuButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ExtensionsMenuButton::GetReferenceButtonForPopup() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->GetExtensionsButton();
}

content::WebContents* ExtensionsMenuButton::GetCurrentWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExtensionsMenuButton::UpdateState() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  SetImageModel(Button::STATE_NORMAL,
                controller_->GetIcon(GetCurrentWebContents(),
                                     gfx::Size(icon_size, icon_size)));

  SetText(controller_->GetActionName());
  SetTooltipText(controller_->GetTooltip(GetCurrentWebContents()));
  SetEnabled(controller_->IsEnabled(GetCurrentWebContents()));

  // The vertical insets need to take into account the icon spacing, since this
  // button's icon is larger, to align with others buttons heights.
  const int vertical_inset =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN) -
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_ICON_SPACING);
  // The horizontal insets reasonably align the extension icons with text inside
  // the dialog with the default button margin.
  const int horizontal_inset =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_inset, horizontal_inset)));
}

void ExtensionsMenuButton::ShowContextMenuAsFallback() {
  // The items in the extensions menu are disabled and unclickable if the
  // primary action cannot be taken; ShowContextMenuAsFallback() should never
  // be called directly.
  NOTREACHED_NORETURN();
}

void ExtensionsMenuButton::ButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.ExtensionActivatedFromMenu"));
  controller_->ExecuteUserAction(
      ToolbarActionViewController::InvocationSource::kMenuEntry);
}

BEGIN_METADATA(ExtensionsMenuButton, views::LabelButton)
END_METADATA
