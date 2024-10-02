// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/login/wildcard_login_checker.h"
#include "chromeos/ash/components/login/auth/auth_metrics_recorder.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/extended_authenticator.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace policy {
class WildcardLoginChecker;
}

namespace ash {

// This class implements chrome-specific elements of Login Performer.

class ChromeLoginPerformer : public LoginPerformer {
 public:
  explicit ChromeLoginPerformer(Delegate* delegate,
                                AuthMetricsRecorder* metrics_recorder);

  ChromeLoginPerformer(const ChromeLoginPerformer&) = delete;
  ChromeLoginPerformer& operator=(const ChromeLoginPerformer&) = delete;

  ~ChromeLoginPerformer() override;

  // LoginPerformer:
  bool IsUserAllowlisted(
      const AccountId& account_id,
      bool* wildcard_match,
      const absl::optional<user_manager::UserType>& user_type) override;

 protected:
  bool RunTrustedCheck(base::OnceClosure callback) override;
  // Runs `callback` unconditionally, but DidRunTrustedCheck() will only be run
  // itself sometimes, so ownership of `callback` should not be held in the
  // Callback pointing to DidRunTrustedCheck.
  void DidRunTrustedCheck(base::OnceClosure* callback);

  void RunOnlineAllowlistCheck(const AccountId& account_id,
                               bool wildcard_match,
                               const std::string& refresh_token,
                               base::OnceClosure success_callback,
                               base::OnceClosure failure_callback) override;

  scoped_refptr<Authenticator> CreateAuthenticator() override;
  bool CheckPolicyForUser(const AccountId& account_id) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory()
      override;

 private:
  void OnlineWildcardLoginCheckCompleted(
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      policy::WildcardLoginChecker::Result result);

  // Used to verify logins that matched wildcard on the login allowlist.
  std::unique_ptr<policy::WildcardLoginChecker> wildcard_login_checker_;
  base::WeakPtrFactory<ChromeLoginPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
