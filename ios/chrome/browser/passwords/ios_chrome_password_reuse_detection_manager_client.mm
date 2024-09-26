// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_reuse_detection_manager_client.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_reuse_manager_factory.h"
#import "ios/chrome/browser/passwords/password_manager_log_router_factory.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"
#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"
#import "ios/chrome/browser/safe_browsing/features.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::metrics_util::PasswordType;

IOSChromePasswordReuseDetectionManagerClient::
    IOSChromePasswordReuseDetectionManagerClient(
        id<IOSChromePasswordReuseDetectionManagerClientBridge> bridge)
    : bridge_(bridge), password_reuse_detection_manager_(this) {
  log_manager_ = autofill::LogManager::Create(
      ios::PasswordManagerLogRouterFactory::GetForBrowserState(
          bridge_.browserState),
      base::RepeatingClosure());

  if (IsPasswordReuseDetectionEnabled()) {
    web_state_observation_.Observe(bridge_.webState);
    input_event_observation_.Observe(
        PasswordProtectionJavaScriptFeature::GetInstance());
  }
}

IOSChromePasswordReuseDetectionManagerClient::
    ~IOSChromePasswordReuseDetectionManagerClient() = default;

password_manager::PasswordReuseManager*
IOSChromePasswordReuseDetectionManagerClient::GetPasswordReuseManager() const {
  return IOSChromePasswordReuseManagerFactory::GetForBrowserState(
      bridge_.browserState);
}

const GURL& IOSChromePasswordReuseDetectionManagerClient::GetLastCommittedURL()
    const {
  return bridge_.lastCommittedURL;
}

autofill::LogManager*
IOSChromePasswordReuseDetectionManagerClient::GetLogManager() {
  return log_manager_.get();
}

safe_browsing::PasswordProtectionService*
IOSChromePasswordReuseDetectionManagerClient::GetPasswordProtectionService()
    const {
  return ChromePasswordProtectionServiceFactory::GetForBrowserState(
      bridge_.browserState);
}

bool IOSChromePasswordReuseDetectionManagerClient::IsSyncAccountEmail(
    const std::string& username) {
  return password_manager::sync_util::IsSyncAccountEmail(
      username,
      IdentityManagerFactory::GetForBrowserState(bridge_.browserState));
}

bool IOSChromePasswordReuseDetectionManagerClient::
    IsPasswordFieldDetectedOnPage() {
  password_manager::PasswordManagerClient* password_manager_client =
      PasswordTabHelper::FromWebState(web_state())->GetPasswordManagerClient();
  if (!password_manager_client) {
    return false;
  }
  return password_manager_client->GetPasswordManager()
             ? password_manager_client->GetPasswordManager()
                   ->IsPasswordFieldDetectedOnPage()
             : false;
}

void IOSChromePasswordReuseDetectionManagerClient::CheckProtectedPasswordEntry(
    PasswordType password_type,
    const std::string& username,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists,
    uint64_t reused_password_hash,
    const std::string& domain) {
  safe_browsing::PasswordProtectionService* service =
      GetPasswordProtectionService();
  if (service) {
    auto show_warning_callback =
        base::BindOnce(&IOSChromePasswordReuseDetectionManagerClient::
                           NotifyUserPasswordProtectionWarning,
                       weak_factory_.GetWeakPtr());
    service->MaybeStartProtectedPasswordEntryRequest(
        bridge_.webState, bridge_.webState->GetLastCommittedURL(), username,
        password_type, matching_reused_credentials, password_field_exists,
        std::move(show_warning_callback));
  }
}

void IOSChromePasswordReuseDetectionManagerClient::
    NotifyUserPasswordProtectionWarning(
        const std::u16string& warning_text,
        base::OnceCallback<void(safe_browsing::WarningAction)> callback) {
  __block auto block_callback = std::move(callback);
  [bridge_
      showPasswordProtectionWarning:base::SysUTF16ToNSString(warning_text)
                         completion:^(safe_browsing::WarningAction action) {
                           std::move(block_callback).Run(action);
                         }];
}

void IOSChromePasswordReuseDetectionManagerClient::
    MaybeLogPasswordReuseDetectedEvent() {
  safe_browsing::PasswordProtectionService* service =
      GetPasswordProtectionService();
  if (service) {
    service->MaybeLogPasswordReuseDetectedEvent(bridge_.webState);
  }
}

void IOSChromePasswordReuseDetectionManagerClient::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  password_reuse_detection_manager_.DidNavigateMainFrame(GetLastCommittedURL());
}

void IOSChromePasswordReuseDetectionManagerClient::OnKeyPressed(
    const std::string text) {
  password_reuse_detection_manager_.OnKeyPressedCommitted(
      base::UTF8ToUTF16(text));
}

void IOSChromePasswordReuseDetectionManagerClient::OnPaste(
    const std::string text) {
  password_reuse_detection_manager_.OnPaste(base::UTF8ToUTF16(text));
}

web::WebState* IOSChromePasswordReuseDetectionManagerClient::web_state() const {
  return bridge_.webState;
}
