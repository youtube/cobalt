// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/test_signin_client.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/account.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

TestWaitForNetworkCallbackHelper::TestWaitForNetworkCallbackHelper() = default;
TestWaitForNetworkCallbackHelper::~TestWaitForNetworkCallbackHelper() = default;

void TestWaitForNetworkCallbackHelper::SetNetworkCallsDelayed(bool delayed) {
  network_calls_delayed_ = delayed;

  if (!network_calls_delayed_) {
    std::vector<base::OnceClosure> callbacks;
    delayed_network_calls_.swap(callbacks);
    for (base::OnceClosure& callback : callbacks) {
      std::move(callback).Run();
    }
  }
}

bool TestWaitForNetworkCallbackHelper::AreNetworkCallsDelayed() {
  return network_calls_delayed_;
}

void TestWaitForNetworkCallbackHelper::DelayNetworkCall(
    base::OnceClosure callback) {
  if (AreNetworkCallsDelayed()) {
    delayed_network_calls_.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

TestSigninClient::TestSigninClient(
    PrefService* pref_service,
    network::TestURLLoaderFactory* test_url_loader_factory)
    : test_wait_for_network_callback_helper_(
          std::make_unique<TestWaitForNetworkCallbackHelper>()),
      test_url_loader_factory_(test_url_loader_factory),
      pref_service_(pref_service),
      are_signin_cookies_allowed_(true) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestSigninClient::GetURLLoaderFactory() {
  return GetTestURLLoaderFactory()->GetSafeWeakWrapper();
}

network::mojom::CookieManager* TestSigninClient::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<network::TestCookieManager>();
  return cookie_manager_.get();
}

network::TestURLLoaderFactory* TestSigninClient::GetTestURLLoaderFactory() {
  if (test_url_loader_factory_)
    return test_url_loader_factory_;

  if (!default_test_url_loader_factory_) {
    default_test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
  }

  return default_test_url_loader_factory_.get();
}

void TestSigninClient::OverrideTestUrlLoaderFactory(
    network::TestURLLoaderFactory* factory) {
  DCHECK(!default_test_url_loader_factory_);
  DCHECK(!test_url_loader_factory_);
  test_url_loader_factory_ = factory;
}

void TestSigninClient::SetNetworkCallsDelayed(bool delayed) {
  test_wait_for_network_callback_helper_->SetNetworkCallsDelayed(delayed);
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

bool TestSigninClient::AreSigninCookiesDeletedOnExit() {
  return false;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {}

bool TestSigninClient::AreNetworkCallsDelayed() {
  return test_wait_for_network_callback_helper_->AreNetworkCallsDelayed();
}

void TestSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  test_wait_for_network_callback_helper_->DelayNetworkCall(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           GetURLLoaderFactory());
}

version_info::Channel TestSigninClient::GetClientChannel() {
  return version_info::Channel::UNKNOWN;
}

void TestSigninClient::OnPrimaryAccountChangedWithEventSource(
    signin::PrimaryAccountChangeEvent event_details,
    absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
        event_source) {}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
absl::optional<account_manager::Account>
TestSigninClient::GetInitialPrimaryAccount() {
  return initial_primary_account_;
}

absl::optional<bool> TestSigninClient::IsInitialPrimaryAccountChild() const {
  return is_initial_primary_account_child_;
}

void TestSigninClient::SetInitialPrimaryAccountForTests(
    const account_manager::Account& account,
    const absl::optional<bool>& is_child) {
  initial_primary_account_ = absl::make_optional(account);
  is_initial_primary_account_child_ = is_child;
}

void TestSigninClient::RemoveAccount(
    const account_manager::AccountKey& account_key) {}
void TestSigninClient::RemoveAllAccounts() {}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
