// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth/chrome_login_performer.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

ChromeLoginPerformer::ChromeLoginPerformer(
    Delegate* delegate,
    AuthMetricsRecorder* metrics_recorder)
    : LoginPerformer(delegate, metrics_recorder) {}

ChromeLoginPerformer::~ChromeLoginPerformer() {}

////////////////////////////////////////////////////////////////////////////////
// ChromeLoginPerformer, public:

bool ChromeLoginPerformer::RunTrustedCheck(base::OnceClosure callback) {
  CrosSettings* cros_settings = CrosSettings::Get();

  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(
          base::BindOnce(&ChromeLoginPerformer::DidRunTrustedCheck,
                         weak_factory_.GetWeakPtr(), &callback));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_) {
      delegate_->PolicyLoadFailed();
    } else {
      NOTREACHED();
    }
    return true;  // Some callback was called.
  } else if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return false;
  } else {
    DCHECK(status == CrosSettingsProvider::TRUSTED);
    // CrosSettingsProvider::TRUSTED
    std::move(callback).Run();
    return true;  // Some callback was called.
  }
}

void ChromeLoginPerformer::DidRunTrustedCheck(base::OnceClosure* callback) {
  CrosSettings* cros_settings = CrosSettings::Get();

  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(
          base::BindOnce(&ChromeLoginPerformer::DidRunTrustedCheck,
                         weak_factory_.GetWeakPtr(), callback));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_) {
      delegate_->PolicyLoadFailed();
    } else {
      NOTREACHED();
    }
  } else if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  } else {
    DCHECK(status == CrosSettingsProvider::TRUSTED);
    std::move(*callback).Run();
  }
}

bool ChromeLoginPerformer::IsUserAllowlisted(
    const AccountId& account_id,
    bool* wildcard_match,
    const absl::optional<user_manager::UserType>& user_type) {
  return CrosSettings::Get()->IsUserAllowlisted(account_id.GetUserEmail(),
                                                wildcard_match, user_type);
}

void ChromeLoginPerformer::RunOnlineAllowlistCheck(
    const AccountId& account_id,
    bool wildcard_match,
    const std::string& refresh_token,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback) {
  // On cloud managed devices, reconfirm login permission with the server.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsCloudManaged() && wildcard_match &&
      (signin::AccountManagedStatusFinder::IsEnterpriseUserBasedOnEmail(
           account_id.GetUserEmail()) ==
       signin::AccountManagedStatusFinder::EmailEnterpriseStatus::kUnknown)) {
    wildcard_login_checker_ = std::make_unique<policy::WildcardLoginChecker>();
    if (refresh_token.empty()) {
      NOTREACHED() << "Refresh token must be present.";
      OnlineWildcardLoginCheckCompleted(
          std::move(success_callback), std::move(failure_callback),
          policy::WildcardLoginChecker::RESULT_FAILED);
    } else {
      wildcard_login_checker_->StartWithRefreshToken(
          refresh_token,
          base::BindOnce(
              &ChromeLoginPerformer::OnlineWildcardLoginCheckCompleted,
              weak_factory_.GetWeakPtr(), std::move(success_callback),
              std::move(failure_callback)));
    }
  } else {
    std::move(success_callback).Run();
  }
}

scoped_refptr<Authenticator> ChromeLoginPerformer::CreateAuthenticator() {
  return UserSessionManager::GetInstance()->CreateAuthenticator(this);
}

bool ChromeLoginPerformer::CheckPolicyForUser(const AccountId& account_id) {
  // Login is not allowed if policy could not be loaded for the account.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  return policy_service &&
         policy_service->IsPolicyAvailableForUser(account_id.GetUserEmail());
}
////////////////////////////////////////////////////////////////////////////////
// ChromeLoginPerformer, private:

scoped_refptr<network::SharedURLLoaderFactory>
ChromeLoginPerformer::GetSigninURLLoaderFactory() {
  return login::GetSigninURLLoaderFactory();
}

void ChromeLoginPerformer::OnlineWildcardLoginCheckCompleted(
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    policy::WildcardLoginChecker::Result result) {
  if (result == policy::WildcardLoginChecker::RESULT_ALLOWED) {
    std::move(success_callback).Run();
  } else {
    std::move(failure_callback).Run();
  }
}

}  // namespace ash
