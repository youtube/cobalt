// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

using content::OriginTrialStatusChangeDetails;

namespace tpcd::trial {

// A profile-keyed service that maintains TPCD_TRIAL content settings based on
// the state of the associated origin trial for a given
// `origin`-`partition_site` pair.
class TopLevelTrialService
    : public content::OriginTrialsControllerDelegate::Observer,
      public KeyedService {
 public:
  explicit TopLevelTrialService(content::BrowserContext* browser_context);
  ~TopLevelTrialService() override;
  TopLevelTrialService(const TopLevelTrialService&) = delete;
  TopLevelTrialService& operator=(const TopLevelTrialService&) = delete;

  // KeyedService overrides:
  void Shutdown() override;

  void UpdateTopLevelTrialSettingsForTesting(const url::Origin& origin,
                                             bool match_subdomains,
                                             bool enabled);

 private:
  // Updates `ContentSettingsForOneType::TOP_LEVEL_TPCD_TRIAL` to reflect
  // the status of the trial for `origin`. If `match_subdomains` is true,
  // a custom scope is used for the content setting to match all subdomains of
  // `origin`.
  void UpdateTopLevelTrialSettings(const url::Origin& origin,
                                   bool match_subdomains,
                                   bool enabled);
  void ClearTopLevelTrialSettings();

  // content::OriginTrialsControllerDelegate::Observer overrides:
  void OnStatusChanged(const OriginTrialStatusChangeDetails& details) override;
  void OnPersistedTokensCleared() override;
  std::string trial_name() override;

  raw_ptr<content::OriginTrialsControllerDelegate> ot_controller_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TOP_LEVEL_TRIAL_SERVICE_H_
