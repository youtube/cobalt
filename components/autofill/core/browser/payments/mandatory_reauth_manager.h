// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MANDATORY_REAUTH_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MANDATORY_REAUTH_MANAGER_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/device_reauth/device_authenticator.h"

namespace autofill {

class AutofillClient;

namespace payments {

enum class MandatoryReauthAuthenticationMethod {
  kUnknown = 0,
  kUnsupportedMethod = 1,
  // Biometric auth is supported on this device, but this does not strictly
  // mean that user is doing biometric auth since user can always fall back to
  // use password.
  kBiometric = 2,
  // This screen lock category excludes biometric above. It normally refers to
  // passcode/PIN/device code.
  kScreenLock = 3,
  kMaxValue = kScreenLock,
};

class MandatoryReauthManager {
 public:
  explicit MandatoryReauthManager(AutofillClient* client);
  MandatoryReauthManager(const MandatoryReauthManager&) = delete;
  MandatoryReauthManager& operator=(const MandatoryReauthManager&) = delete;
  virtual ~MandatoryReauthManager();

  // Initiates an authentication flow. This method calls
  // `DeviceAuthenticator::Authenticate`, which is only implemented on Android.
  // It will create a new instance of `device_authenticator_`, which will be
  // reset once the authentication is finished. This method ensures that the
  // last valid DeviceAuthenticator authentication is used if it happened within
  // the set default auth validity period.
  virtual void Authenticate(
      device_reauth::DeviceAuthenticator::AuthenticateCallback callback);

  // Initiates an authentication flow. This method calls
  // `DeviceAuthenticator::AuthenticateWithMessage`, which is only implemented
  // on certain desktop platforms. It will create a new instance of
  // `device_authenticator_`, which will be reset once the authentication is
  // finished.
  virtual void AuthenticateWithMessage(
      const std::u16string& message,
      device_reauth::DeviceAuthenticator::AuthenticateCallback callback);

  // This method is triggered once an authentication flow is completed. It will
  // reset `device_authenticator_` before triggering `callback` with `success`.
  void OnAuthenticationCompleted(
      device_reauth::DeviceAuthenticator::AuthenticateCallback callback,
      bool success);

  // Returns true if the user conditions denote that we should offer opt-in for
  // this user, false otherwise.
  // `card_record_type_if_non_interactive_authentication_flow_completed` will be
  // present if a payments autofill occurred with non-interactive
  // authentication, and will hold the record type of the card that had the most
  // recent non-interactive authentication.
  virtual bool ShouldOfferOptin(
      absl::optional<CreditCard::RecordType>
          card_record_type_if_non_interactive_authentication_flow_completed);

  // Starts the opt-in flow. This flow includes an opt-in bubble, an
  // authentication step, and then a confirmation bubble. This function should
  // only be called after we have checked that we should offer opt-in by calling
  // `ShouldOfferOptin()`.
  virtual void StartOptInFlow();

  // Triggered when the user accepts the opt-in prompt. This will initiate an
  // authentication.
  virtual void OnUserAcceptedOptInPrompt();

  // Triggered when the user completes the authentication step in
  // the opt-in flow. If this is successful, it will enroll the user into
  // mandatory re-auth, and display a confirmation bubble. Otherwise it will
  // increment the promo shown counter.
  virtual void OnOptInAuthenticationStepCompleted(bool success);

  // Triggered when the user cancels the opt-in prompt.
  virtual void OnUserCancelledOptInPrompt();

  // Triggered when the user closes the opt-in prompt.
  virtual void OnUserClosedOptInPrompt();

  // Return the authentication method to be used on this device. Used for metric
  // logging.
  virtual MandatoryReauthAuthenticationMethod GetAuthenticationMethod();

  device_reauth::DeviceAuthenticator* GetDeviceAuthenticatorPtrForTesting() {
    return device_authenticator_.get();
  }

 private:
  // Raw pointer to the web content's AutofillClient.
  raw_ptr<AutofillClient> client_;

  // Used for authentication related to mandatory re-auth.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  // Used to store the opt in source for logging purposes.
  autofill_metrics::MandatoryReauthOptInOrOutSource opt_in_source_ =
      autofill_metrics::MandatoryReauthOptInOrOutSource::kUnknown;

  base::WeakPtrFactory<MandatoryReauthManager> weak_ptr_factory_{this};
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MANDATORY_REAUTH_MANAGER_H_
