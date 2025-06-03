// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
class RepeatingTimer;
class SequencedTaskRunner;
class TimeDelta;
}  // namespace base

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

namespace policy {

class UserCloudPolicyManagerAsh;

// A PKS that observes an IdentityManager and mints the policy access
// token for the UserCloudPolicyManagerAsh. First token is fetched when the
// token service becomes ready. After that if needed a new token is fetched when
// the previous one is expected to expire. This service decouples the
// UserCloudPolicyManagerAsh from depending directly on the
// IdentityManager, since it is initialized much earlier.
class UserCloudPolicyTokenForwarder : public KeyedService,
                                      public CloudPolicyService::Observer {
 public:
  // Backoff policy for token fetch retry attempts in case token fetch failed or
  // returned invalid data.
  static const net::BackoffEntry::Policy kFetchTokenRetryBackoffPolicy;

  // Histogram to log errors occurred while fetching OAuth token for child user.
  static constexpr char kUMAChildUserOAuthTokenError[] =
      "Enterprise.UserPolicyChromeOS.ChildUser.OAuthTokenError";

  // The factory of this PKS depends on the factories of these two arguments,
  // so this object will be Shutdown() first and these pointers can be used
  // until that point.
  UserCloudPolicyTokenForwarder(UserCloudPolicyManagerAsh* manager,
                                signin::IdentityManager* identity_manager);

  UserCloudPolicyTokenForwarder(const UserCloudPolicyTokenForwarder&) = delete;
  UserCloudPolicyTokenForwarder& operator=(
      const UserCloudPolicyTokenForwarder&) = delete;

  ~UserCloudPolicyTokenForwarder() override;

  // KeyedService:
  void Shutdown() override;

  // CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;

  // Returns whether OAuth token fetch is currently in progress.
  bool IsTokenFetchInProgressForTesting() const;

  // Returns whether next OAuth token refresh is scheduled.
  bool IsTokenRefreshScheduledForTesting() const;

  // Returns delay to next token refresh if it is scheduled.
  absl::optional<base::TimeDelta> GetTokenRefreshDelayForTesting() const;

  // Overrides elements responsible for time progression to allow testing.
  // Affects time calculation and timer objects.
  void OverrideTimeForTesting(
      const base::Clock* clock,
      const base::TickClock* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void StartRequest();
  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo token_info);

  raw_ptr<UserCloudPolicyManagerAsh, ExperimentalAsh> manager_;
  raw_ptr<signin::IdentityManager, DanglingUntriaged | ExperimentalAsh>
      identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Last fetched OAuth token.
  absl::optional<signin::AccessTokenInfo> oauth_token_;

  // Timer that measures time to the next OAuth token refresh. Not initialized
  // if token refresh is not scheduled.
  std::unique_ptr<base::RepeatingTimer> refresh_oauth_token_timer_;

  // Backoff for fetch token retry attempts.
  std::unique_ptr<net::BackoffEntry> retry_backoff_;

  // Points to the base::DefaultClock by default.
  raw_ptr<const base::Clock, ExperimentalAsh> clock_;

  base::WeakPtrFactory<UserCloudPolicyTokenForwarder> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
