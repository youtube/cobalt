// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

DEFINE_USER_DATA(OmniboxAutofillBubbleController);

OmniboxAutofillBubbleController::OmniboxAutofillBubbleController(
    tabs::TabInterface& tab_interface,
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

OmniboxAutofillBubbleController::~OmniboxAutofillBubbleController() = default;

void OmniboxAutofillBubbleController::Initialize(
    std::vector<Suggestion> suggestions,
    base::RepeatingCallback<void(base::span<const Suggestion>)>
        on_suggestions_shown,
    base::RepeatingCallback<void(const Suggestion&)> did_select_suggestion,
    base::RepeatingCallback<
        void(const Suggestion&,
             const AutofillSuggestionDelegate::SuggestionMetadata&)>
        did_accept_suggestion) {
  suggestions_ = std::move(suggestions);
  on_suggestions_shown_callback_ = std::move(on_suggestions_shown);
  did_select_suggestion_callback_ = std::move(did_select_suggestion);
  did_accept_suggestion_callback_ = std::move(did_accept_suggestion);
}

// static
OmniboxAutofillBubbleController* OmniboxAutofillBubbleController::From(
    tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

BubbleType OmniboxAutofillBubbleController::GetBubbleType() const {
  return BubbleType::kOmniboxAutofill;
}

base::WeakPtr<BubbleControllerBase>
OmniboxAutofillBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OmniboxAutofillBubbleController::DoShowBubble() {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents());
  if (!browser) {
    return;
  }
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  if (!browser_window) {
    return;
  }
  if (AutofillBubbleBase* bubble_view =
          browser_window->GetAutofillBubbleHandler()->ShowOmniboxAutofillBubble(
              web_contents(), this)) {
    SetBubbleView(*bubble_view);
  }
}

void OmniboxAutofillBubbleController::OnBubbleClosed(
    PaymentsUiClosedReason reason) {
  ResetBubbleViewAndInformBubbleManager();
}

base::WeakPtr<OmniboxAutofillBubbleController>
OmniboxAutofillBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
