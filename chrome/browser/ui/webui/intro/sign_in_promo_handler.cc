// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/sign_in_promo_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/intro/policy_store_observer.h"
#include "chrome/browser/ui/webui/intro/sign_in_promo.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

SignInPromoHandler::SignInPromoHandler(
    base::RepeatingCallback<void(IntroChoice)> signin_choice_callback,
    bool is_device_managed,
    mojo::PendingRemote<intro::mojom::SignInPromoPage> page,
    mojo::PendingReceiver<intro::mojom::SignInPromoPageHandler> receiver)
    : signin_choice_callback_(std::move(signin_choice_callback)),
      is_device_managed_(is_device_managed),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  CHECK(signin_choice_callback_);
  if (is_device_managed_) {
    policy_store_observer_ = std::make_unique<PolicyStoreObserver>(
        base::BindOnce(&SignInPromoHandler::OnDisclaimerFetched,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

SignInPromoHandler::~SignInPromoHandler() = default;

void SignInPromoHandler::GetManagedDeviceDisclaimer(
    GetManagedDeviceDisclaimerCallback callback) {
  if (!is_device_managed_) {
    std::move(callback).Run("");
    return;
  }
  if (managed_device_disclaimer_.has_value()) {
    std::move(callback).Run(*managed_device_disclaimer_);
    return;
  }
  pending_disclaimer_callback_ = std::move(callback);
}

void SignInPromoHandler::ContinueWithAccount() {
  signin_choice_callback_.Run(IntroChoice::kContinueWithAccount);
}

void SignInPromoHandler::ContinueWithoutAccount() {
  signin_choice_callback_.Run(IntroChoice::kContinueWithoutAccount);
}

void SignInPromoHandler::ResetIntroButtons() {
  page_->OnResetButtons();
}

void SignInPromoHandler::OnDisclaimerFetched(std::string disclaimer) {
  CHECK(is_device_managed_);
  managed_device_disclaimer_ = std::move(disclaimer);
  if (pending_disclaimer_callback_) {
    std::move(pending_disclaimer_callback_).Run(*managed_device_disclaimer_);
  }
}
