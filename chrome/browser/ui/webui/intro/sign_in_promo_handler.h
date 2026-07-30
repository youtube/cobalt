// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_PROMO_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/intro/sign_in_promo.mojom.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

enum class IntroChoice;

class SignInPromoHandler : public intro::mojom::SignInPromoPageHandler {
 public:
  SignInPromoHandler(
      base::RepeatingCallback<void(IntroChoice)> signin_choice_callback,
      bool is_device_managed,
      mojo::PendingRemote<intro::mojom::SignInPromoPage> page,
      mojo::PendingReceiver<intro::mojom::SignInPromoPageHandler> receiver);

  SignInPromoHandler(const SignInPromoHandler&) = delete;
  SignInPromoHandler& operator=(const SignInPromoHandler&) = delete;

  ~SignInPromoHandler() override;

  // intro::mojom::SignInPromoPageHandler:
  void GetManagedDeviceDisclaimer(
      GetManagedDeviceDisclaimerCallback callback) override;
  void ContinueWithAccount() override;
  void ContinueWithoutAccount() override;

  // Requests the WebUI to reset the buttons state.
  void ResetIntroButtons();

 private:
  // Callback invoked by `PolicyStoreObserver` when the disclaimer is fetched.
  void OnDisclaimerFetched(std::string disclaimer);

  const base::RepeatingCallback<void(IntroChoice)> signin_choice_callback_;
  const bool is_device_managed_;

  std::optional<std::string> managed_device_disclaimer_;
  GetManagedDeviceDisclaimerCallback pending_disclaimer_callback_;

  std::unique_ptr<policy::CloudPolicyStore::Observer> policy_store_observer_;

  mojo::Receiver<intro::mojom::SignInPromoPageHandler> receiver_;
  mojo::Remote<intro::mojom::SignInPromoPage> page_;

  base::WeakPtrFactory<SignInPromoHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_SIGN_IN_PROMO_HANDLER_H_
