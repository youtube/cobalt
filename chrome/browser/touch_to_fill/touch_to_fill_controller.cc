// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller_delegate.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/url_formatter/elide_url.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using password_manager::PasskeyCredential;
using password_manager::UiCredential;

std::vector<UiCredential> SortCredentials(
    base::span<const UiCredential> credentials) {
  std::vector<UiCredential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) Prefer non-PSL matches over PSL matches.
  // 2) Prefer credentials that were used recently over others.
  //
  // Note: This ordering matches password_manager_util::FindBestMatches().
  base::ranges::sort(result, std::greater<>{}, [](const UiCredential& cred) {
    return std::make_pair(!cred.is_public_suffix_match(), cred.last_used());
  });

  return result;
}

}  // namespace

TouchToFillController::TouchToFillController() = default;
TouchToFillController::~TouchToFillController() = default;

void TouchToFillController::Show(
    base::span<const UiCredential> credentials,
    base::span<PasskeyCredential> passkey_credentials,
    std::unique_ptr<TouchToFillControllerDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);

  delegate_->OnShow(credentials, passkey_credentials);
  if (credentials.empty() && passkey_credentials.empty()) {
    // Ideally this should never happen. However, in case we do end up invoking
    // Show() without credentials, we should not show Touch To Fill to the user
    // and treat this case as dismissal, in order to restore the soft keyboard.
    OnDismiss();
    return;
  }

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  GURL url = delegate_->GetFrameUrl();
  view_->Show(
      url,
      TouchToFillView::IsOriginSecure(
          network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url))),
      SortCredentials(credentials), passkey_credentials,
      delegate_->ShouldTriggerSubmission(),
      password_manager_launcher::CanManagePasswordsWhenPasskeysPresent());
}

void TouchToFillController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnPasskeyCredentialSelected(
    const PasskeyCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnPasskeyCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnManagePasswordsSelected(bool passkeys_shown) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnManagePasswordsSelected(
      passkeys_shown, base::BindOnce(&TouchToFillController::ActionCompleted,
                                     base::Unretained(this)));
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnDismiss(base::BindOnce(&TouchToFillController::ActionCompleted,
                                      base::Unretained(this)));
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return delegate_->GetNativeView();
}

void TouchToFillController::Close() {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnDismiss(base::BindOnce(&TouchToFillController::ActionCompleted,
                                      base::Unretained(this)));
}

void TouchToFillController::ActionCompleted() {
  delegate_.reset();
}
