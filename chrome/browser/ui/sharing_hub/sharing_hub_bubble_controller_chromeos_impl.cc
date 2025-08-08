// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller_chromeos_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/button/button.h"

namespace sharing_hub {

// static
SharingHubBubbleController*
SharingHubBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SharingHubBubbleControllerChromeOsImpl::CreateForWebContents(web_contents);
  SharingHubBubbleControllerChromeOsImpl* controller =
      SharingHubBubbleControllerChromeOsImpl::FromWebContents(web_contents);
  return controller;
}

SharingHubBubbleControllerChromeOsImpl::
    ~SharingHubBubbleControllerChromeOsImpl() {
  if (bubble_showing_) {
    bubble_showing_ = false;
    // Close any remnant Sharesheet dialog.
    CloseSharesheet();

    // We must deselect the icon manually since the Sharesheet will not be able
    // to invoke OnSharesheetClosed() at this point.
    DeselectIcon();
  }
}

void SharingHubBubbleControllerChromeOsImpl::HideBubble() {
  if (bubble_showing_) {
    CloseSharesheet();
  }
}

void SharingHubBubbleControllerChromeOsImpl::ShowBubble(
    share::ShareAttempt attempt) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  // Ignore subsequent calls to open the Sharesheet if it already is open. This
  // is especially for the Nearby Share dialog, where clicking outside of it
  // will not dismiss the dialog.
  if (bubble_showing_)
    return;
  bubble_showing_ = true;
  ShowSharesheet(browser->window()->GetSharingHubIconButton());

  share::LogShareSourceDesktop(share::ShareSourceDesktop::kOmniboxSharingHub);
}

SharingHubBubbleView*
SharingHubBubbleControllerChromeOsImpl::sharing_hub_bubble_view() const {
  return nullptr;
}

bool SharingHubBubbleControllerChromeOsImpl::ShouldOfferOmniboxIcon() {
  return !GetProfile()->IsIncognitoProfile() && !GetProfile()->IsGuestSession();
}

Profile* SharingHubBubbleControllerChromeOsImpl::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

void SharingHubBubbleControllerChromeOsImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  // Cancel the current share attempt if the user switches to a different tab in
  // the window. Switching windows is permitted since a Sharesheet is tied to
  // the native window.
  if (bubble_showing_ && visibility == content::Visibility::HIDDEN) {
    CloseSharesheet();
  }
}

void SharingHubBubbleControllerChromeOsImpl::ShowSharesheet(
    views::Button* highlighted_button) {
  DCHECK(highlighted_button);
  highlighted_button_tracker_.SetView(highlighted_button);

  ShowSharesheetAsh();

  // Save the window in order to close the Sharesheet if the tab is closed. This
  // will return the incorrect window if called later.
  parent_window_ = GetWebContents().GetTopLevelNativeWindow();
  parent_window_tracker_ = views::NativeWindowTracker::Create(parent_window_);
}

void SharingHubBubbleControllerChromeOsImpl::CloseSharesheet() {
  if (parent_window_ && !parent_window_tracker_->WasNativeWindowDestroyed()) {
    CloseSharesheetAsh();
  }

  // OnSharesheetClosed() is not guaranteed to be called by the
  // SharesheetController (specifically for the case where this is invoked by
  // our destructor). Hence, we must explicitly set this null here.
  parent_window_ = nullptr;
  parent_window_tracker_ = nullptr;
}

sharesheet::SharesheetService*
SharingHubBubbleControllerChromeOsImpl::GetSharesheetService() {
  Profile* const profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (!profile)
    return nullptr;

  return sharesheet::SharesheetServiceFactory::GetForProfile(profile);
}

void SharingHubBubbleControllerChromeOsImpl::ShowSharesheetAsh() {
  sharesheet::SharesheetService* sharesheet_service = GetSharesheetService();
  if (!sharesheet_service)
    return;

  apps::IntentPtr intent = apps_util::MakeShareIntent(
      GetWebContents().GetLastCommittedURL().spec(),
      base::UTF16ToUTF8(GetWebContents().GetTitle()));
  sharesheet_service->ShowBubble(
      &GetWebContents(), std::move(intent),
      sharesheet::LaunchSource::kOmniboxShare,
      base::BindOnce([](sharesheet::SharesheetResult result) {}),
      base::BindOnce(
          &SharingHubBubbleControllerChromeOsImpl::OnSharesheetClosed,
          weak_ptr_factory_.GetWeakPtr()));
}

void SharingHubBubbleControllerChromeOsImpl::CloseSharesheetAsh() {
  sharesheet::SharesheetService* sharesheet_service = GetSharesheetService();
  if (!sharesheet_service)
    return;

  sharesheet::SharesheetController* sharesheet_controller =
      sharesheet_service->GetSharesheetController(parent_window_);
  if (!sharesheet_controller)
    return;

  sharesheet_controller->CloseBubble(sharesheet::SharesheetResult::kCancel);
}

void SharingHubBubbleControllerChromeOsImpl::OnSharesheetClosed(
    views::Widget::ClosedReason reason) {
  bubble_showing_ = false;
  parent_window_ = nullptr;
  parent_window_tracker_ = nullptr;
  // Deselect the omnibox icon now that the Sharesheet is closed.
  DeselectIcon();
}

void SharingHubBubbleControllerChromeOsImpl::DeselectIcon() {
  if (!highlighted_button_tracker_.view())
    return;

  views::Button* button =
      views::Button::AsButton(highlighted_button_tracker_.view());
  if (button)
    button->SetHighlighted(false);
}

SharingHubBubbleControllerChromeOsImpl::SharingHubBubbleControllerChromeOsImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SharingHubBubbleControllerChromeOsImpl>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleControllerChromeOsImpl);

}  // namespace sharing_hub
