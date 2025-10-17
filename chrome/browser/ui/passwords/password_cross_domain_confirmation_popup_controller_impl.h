// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_interface.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/password_manager/core/browser/password_cross_domain_confirmation_popup_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

class PasswordCrossDomainConfirmationPopupView;

// Implementation of the cross domain usage confirmation popup controller.
class PasswordCrossDomainConfirmationPopupControllerImpl
    : public PasswordCrossDomainConfirmationPopupControllerInterface,
      public content::WebContentsObserver {
 public:
  using ViewFactoryForTesting = base::RepeatingCallback<
      base::WeakPtr<PasswordCrossDomainConfirmationPopupView>(
          base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
          const GURL& domain,
          const std::u16string& password_hostname,
          base::OnceClosure confirmation_callback,
          base::OnceClosure cancel_callback)>;

  explicit PasswordCrossDomainConfirmationPopupControllerImpl(
      content::WebContents* web_contents);
  PasswordCrossDomainConfirmationPopupControllerImpl(
      const PasswordCrossDomainConfirmationPopupControllerImpl&) = delete;
  PasswordCrossDomainConfirmationPopupControllerImpl& operator=(
      const PasswordCrossDomainConfirmationPopupControllerImpl&) = delete;
  ~PasswordCrossDomainConfirmationPopupControllerImpl() override;

  // Creates and shows a popup pointing to `element_bounds` and presenting
  // a message regarding cross domain password usage. `domain` is the domain
  // of the current web site the popup is triggered on. `password_domain` is
  // the domain of the web site the password was originally stored on.
  // `confirmation_callback` is called if the user confirms the action, if
  // the user cancels it, the popup is silently hidden.
  // If the popup is already shown, it gets hidden and a new one shows up.
  void Show(const gfx::RectF& element_bounds,
            base::i18n::TextDirection text_direction,
            const GURL& domain,
            std::u16string password_hostname,
            base::OnceClosure confirmation_callback,
            bool show_warning_text);

  // autofill::AutofillPopupViewDelegate:
  void Hide(autofill::SuggestionHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  autofill::PopupAnchorType anchor_type() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  std::u16string GetBodyText() const override;

  std::u16string GetTitleText() const override;

  // content::WebContentsObserver:
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  void set_view_factory_for_testing(ViewFactoryForTesting factory) {
    view_factory_for_testing_ = std::move(factory);
  }

 private:
  void HideImpl(CrossDomainPasswordFillingConfirmation result);
  bool OverlapsWithPictureInPictureWindow() const;

  // Handles the confirmation response from the view.
  void OnConfirm();

  // Handles the cancel response from the view.
  void OnCancel();

  gfx::RectF element_bounds_;
  base::i18n::TextDirection text_direction_;
  base::OnceClosure confirmation_callback_;
  // The hostname (either website or Android app name) that the password
  // associated with.
  std::u16string password_hostname_;
  // The current website where filling is being triggered on.
  GURL domain_;
  // If true, the text of the popup should be a warning since the cross domain
  // filling suggestion is being pushed to the user.
  // If false, the text of the popup should be informative since the cross
  // domain filling suggestion is being pulled by the request of the user.
  bool show_warning_text_ = false;

  base::WeakPtr<PasswordCrossDomainConfirmationPopupView> view_;

  std::optional<autofill::AutofillPopupHideHelper> popup_hide_helper_;

  ViewFactoryForTesting view_factory_for_testing_;

  base::WeakPtrFactory<PasswordCrossDomainConfirmationPopupControllerImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_IMPL_H_
