// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"

#import "base/functional/callback_helpers.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

GeminiCapabilitiesManagerImpl::GeminiCapabilitiesManagerImpl(
    signin::IdentityManager* identity_manager,
    AuthenticationService* authentication_service,
    GeminiService* gemini_service)
    : identity_manager_(identity_manager),
      authentication_service_(authentication_service),
      gemini_service_(gemini_service) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
  // Update capabilities immediately upon initialization.
  UpdateCapabilities();
}

GeminiCapabilitiesManagerImpl::~GeminiCapabilitiesManagerImpl() = default;

void GeminiCapabilitiesManagerImpl::Shutdown() {
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  authentication_service_ = nullptr;
  gemini_service_ = nullptr;
}

void GeminiCapabilitiesManagerImpl::UpdateCapabilities() {
  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();

  // If the feature is disabled, clean up all capabilities and return early.
  if (!IsAppSwitcherAISummarizationEnabled()) {
    [shared_defaults removeObjectForKey:app_group::kAppSwitcherHashedUserID];

    NSDictionary* existing_capabilities = [shared_defaults
        dictionaryForKey:app_group::kChromeCapabilitiesPreference];
    if (existing_capabilities) {
      NSMutableDictionary* capabilities = [existing_capabilities mutableCopy];
      [capabilities removeObjectForKey:
                        app_group::kChromeSupportsAISummarizationCapability];
      [capabilities removeObjectForKey:
                        app_group::kChromeUserIsEligibleForGeminiCapability];
      [shared_defaults setObject:capabilities
                          forKey:app_group::kChromeCapabilitiesPreference];
    }
    return;
  }

  NSDictionary* existing_capabilities = [shared_defaults
      dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  NSMutableDictionary* capabilities = existing_capabilities
                                          ? [existing_capabilities mutableCopy]
                                          : [NSMutableDictionary dictionary];

  bool has_primary_identity =
      authentication_service_ && authentication_service_->HasPrimaryIdentity();
  UpdateSupportsAISummarization(capabilities);
  UpdateHashedUserID(shared_defaults, has_primary_identity);
  UpdateUserEligibility(shared_defaults, capabilities, has_primary_identity);
}

#pragma mark - signin::IdentityManager::Observer

void GeminiCapabilitiesManagerImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateCapabilities();
}

#pragma mark - Private

void GeminiCapabilitiesManagerImpl::UpdateSupportsAISummarization(
    NSMutableDictionary* capabilities) {
  capabilities[app_group::kChromeSupportsAISummarizationCapability] =
      @(IsAppSwitcherAISummarizationEnabled());
}

void GeminiCapabilitiesManagerImpl::UpdateHashedUserID(
    NSUserDefaults* shared_defaults,
    bool has_primary_identity) {
  if (!has_primary_identity) {
    [shared_defaults removeObjectForKey:app_group::kAppSwitcherHashedUserID];
    return;
  }
  id<SystemIdentity> identity = authentication_service_->GetPrimaryIdentity();
  [shared_defaults setObject:identity.hashedGaiaID
                      forKey:app_group::kAppSwitcherHashedUserID];
}

void GeminiCapabilitiesManagerImpl::UpdateUserEligibility(
    NSUserDefaults* shared_defaults,
    NSMutableDictionary* capabilities,
    bool has_primary_identity) {
  bool eligible = false;
  if (has_primary_identity && gemini_service_) {
    eligible = gemini_service_->IsProfileEligibleForGemini();
  }

  capabilities[app_group::kChromeUserIsEligibleForGeminiCapability] =
      @(eligible);

  [shared_defaults setObject:capabilities
                      forKey:app_group::kChromeCapabilitiesPreference];
}
