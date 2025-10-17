// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include "content/public/browser/navigation_handle.h"

HatsService::SurveyMetadata::SurveyMetadata() = default;

HatsService::SurveyMetadata::~SurveyMetadata() = default;

HatsService::SurveyOptions::SurveyOptions(
    std::optional<std::u16string> custom_invitation,
    std::optional<messages::MessageIdentifier> message_identifier)
    : custom_invitation(custom_invitation),
      message_identifier(message_identifier) {}
HatsService::SurveyOptions::SurveyOptions(const SurveyOptions& other) = default;
HatsService::SurveyOptions::~SurveyOptions() = default;

HatsService::HatsService(Profile* profile) : profile_(profile) {
  hats::GetActiveSurveyConfigs(survey_configs_by_triggers_);
}

HatsService::~HatsService() = default;

hats::SurveyConfigs& HatsService::GetSurveyConfigsByTriggersForTesting() {
  return survey_configs_by_triggers_;
}

bool HatsService::IsNavigationAllowed(
    content::NavigationHandle* navigation_handle,
    HatsService::NavigationBehaviour navigation_behaviour) {
  if (navigation_behaviour == NavigationBehaviour::ALLOW_ANY ||
      !navigation_handle || !navigation_handle->IsInPrimaryMainFrame()) {
    return true;
  }

  if (navigation_behaviour == NavigationBehaviour::REQUIRE_SAME_DOCUMENT &&
      navigation_handle->IsSameDocument()) {
    return true;
  }

  if (navigation_behaviour == NavigationBehaviour::REQUIRE_SAME_ORIGIN &&
      navigation_handle->HasCommitted() && navigation_handle->IsSameOrigin()) {
    return true;
  }

  return false;
}
