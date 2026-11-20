// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"

class AuthenticationService;
class GeminiService;
@class NSMutableDictionary;
@class NSUserDefaults;

class GeminiCapabilitiesManagerImpl : public GeminiCapabilitiesManager,
                                      public signin::IdentityManager::Observer {
 public:
  GeminiCapabilitiesManagerImpl(signin::IdentityManager* identity_manager,
                                AuthenticationService* authentication_service,
                                GeminiService* gemini_service);
  ~GeminiCapabilitiesManagerImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // GeminiCapabilitiesManager implementation.
  void UpdateCapabilities() override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  // Helper methods to update specific capabilities.
  void UpdateSupportsAISummarization(NSMutableDictionary* capabilities);
  void UpdateHashedUserID(NSUserDefaults* shared_defaults,
                          bool has_primary_identity);
  void UpdateUserEligibility(NSUserDefaults* shared_defaults,
                             NSMutableDictionary* capabilities,
                             bool has_primary_identity);

  // IdentityManager observed for primary account changes.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // AuthenticationService used to retrieve primary identity.
  raw_ptr<AuthenticationService> authentication_service_;

  // GeminiService used to query user eligibility.
  raw_ptr<GeminiService> gemini_service_;

  // Scoped observation for IdentityManager.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CAPABILITIES_MANAGER_IMPL_H_
