// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Service which receives events from Trust & Safety features and determines
// whether or not to launch a HaTS survey on the NTP for the user.
class TrustSafetySentimentService
    : public KeyedService,
      public ProfileObserver,
      public metrics::DesktopSessionDurationTracker::Observer {
 public:
  explicit TrustSafetySentimentService(Profile* profile);
  ~TrustSafetySentimentService() override;

  // Called when the user opens an NTP. This allows the service to update its
  // eligibility logic, and potentially show a survey. Virtual to allow mocking
  // in tests.
  virtual void OpenedNewTabPage();

  // Called when the user interacts with the privacy settings on
  // chrome://settings in |web_contents|. Interaction in this context could be
  // using a link row on the privacy settings card. Calling this allows the
  // service to monitor |web_contents| to determine if the user stays on
  // settings for the required time. Virtual to allow mocking in tests.
  virtual void InteractedWithPrivacySettings(
      content::WebContents* web_contents);

  // Called when the user runs safety check. This is immediately considered as a
  // trigger action. Virtual to allow mocking in tests.
  virtual void RanSafetyCheck();

  // Called when the user opens Page Info.
  virtual void PageInfoOpened();

  // Called when the user interacts in some way with Page Info.
  virtual void InteractedWithPageInfo();

  // Called when the user saves a password via the manage passwords UI. This is
  // the native UI shown when Chrome detects a password has been entered into
  // the web page.
  virtual void SavedPassword();

  // Called when the user closes Page Info. If Page Info was opened for the
  // target time, or the user interacted with it while it was open, a trigger
  // action is recorded.
  virtual void PageInfoClosed();

  // Called when the user visits chrome://settings/passwords. Calling this
  // allows the service to monitor |web_contents| to determine if the user
  // remains on settings after visiting the page for the required time. Virtual
  // to allow mocking in tests.
  virtual void OpenedPasswordManager(content::WebContents* web_contents);

  // Called when the user saves a card through the native UI bubble shown after
  // the user uses a card on a website.
  virtual void SavedCard();

  // Called when the user runs password check. Virtual to allow mocking in
  // tests.
  virtual void RanPasswordCheck();

  // Called when the user deletes data from Clear Browsing Data dialog.
  virtual void ClearedBrowsingData(browsing_data::BrowsingDataType datatype);

  // Called when the user finishes the privacy guide. Virtual to allow mocking
  // in tests.
  virtual void FinishedPrivacyGuide();

  // Profile Observer:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // metrics::DesktopSessionDurationTracker::Observer
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  // The feature areas that the service delivers HaTS surveys for. Each feature
  // area is associated with a different Listnr survey, and has a different set
  // of Product Specific Data (PSD). kIneligible is an exception, and
  // is not associated with any survey, but rather represents the collection of
  // features for which interaction with should also be considered when
  // determining elibigility for a survey.

  // These values are persisted to logs and entries should not be renumbered or
  // reused and kept up to date with TrustSafetySentimentFeatureArea in
  // enums.xml.
  enum class FeatureArea {
    kIneligible = 0,
    kPrivacySettings = 1,
    kTrustedSurface = 2,
    kTransactions = 3,
    kPrivacySandbox3ConsentAccept = 4,
    kPrivacySandbox3ConsentDecline = 5,
    kPrivacySandbox3NoticeDismiss = 6,
    kPrivacySandbox3NoticeOk = 7,
    kPrivacySandbox3NoticeSettings = 8,
    kPrivacySandbox3NoticeLearnMore = 9,
    kSafetyCheck = 10,
    kPasswordCheck = 11,
    kBrowsingData = 12,
    kPrivacyGuide = 13,
    kControlGroup = 14,
    kPrivacySandbox4ConsentAccept = 15,
    kPrivacySandbox4ConsentDecline = 16,
    kPrivacySandbox4NoticeOk = 17,
    kPrivacySandbox4NoticeSettings = 18,
    kMaxValue = kPrivacySandbox4NoticeSettings,
  };

  // Called when the user interacts with Privacy Sandbox 3, |feature_area|
  // specifies what type of interaction occurred.
  virtual void InteractedWithPrivacySandbox3(FeatureArea feature_area);

  virtual void InteractedWithPrivacySandbox4(FeatureArea feature_area);

  // Checks that this feature area is valid for the current version.
  static bool VersionCheck(FeatureArea feature_area);

  // Gets the HaTS trigger for a feature area.
  static std::string GetHatsTriggerForFeatureArea(FeatureArea feature_area);

  // Performs a FeatureArea and Version-specific dice roll.
  // Returns true if succeeds, else false.
  static bool ProbabilityCheck(FeatureArea feature_area);

 private:
  friend class TrustSafetySentimentServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           Eligibility_NtpOpens);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, Eligibility_Time);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, TriggerProbability);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           TriggersClearOnLaunch);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           SettingsWatcher_PrivacySettings);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           SettingsWatcher_PasswordManager);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, RanSafetyCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           PrivacySettingsProductSpecificData);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           InteractedWithPrivacySandbox3ConsentAccept);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           Eligibility_V1FeatureWhileV2Enabled);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_SafetyCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_TrustedSurface);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_PasswordCheck);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_BrowsingData);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest,
                           V2_BrowsingData_NotInterested);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_PrivacyGuide);
  FRIEND_TEST_ALL_PREFIXES(TrustSafetySentimentServiceTest, V2_ControlGroup);

  // Struct representing a trigger (user action relevant to T&S) that previously
  // occurred, and is awaiting the appropriate eligibility steps before causing
  // a survey to be shown.
  struct PendingTrigger {
    PendingTrigger();
    PendingTrigger(const PendingTrigger& other);
    PendingTrigger(const std::map<std::string, bool>& product_specific_data,
                   int remaining_ntps_to_open);
    explicit PendingTrigger(int remaining_ntps_to_open);
    ~PendingTrigger();

    std::map<std::string, bool> product_specific_data;
    int remaining_ntps_to_open;
    base::Time occurred_time;
  };

  // Class which observes the provided |web_contents| for |required_open_time|
  // and then checks if |web_contents| is currently visible, and has settings
  // open. Calls |success_callback| if the user stays on settings for the
  // required time, calls |complete_callback| when the observation time has
  // expired, or |web_contents| has been destroyed.
  class SettingsWatcher : content::WebContentsObserver {
   public:
    SettingsWatcher(content::WebContents* web_contents,
                    base::TimeDelta required_open_time,
                    base::OnceCallback<void()> success_callback,
                    base::OnceCallback<void()> complete_callback);
    ~SettingsWatcher() override;

    // WebContentsObserver:
    void WebContentsDestroyed() override;

   private:
    void TimerComplete();

    raw_ptr<content::WebContents> web_contents_;
    base::OnceCallback<void()> success_callback_;
    base::OnceCallback<void()> complete_callback_;
    base::WeakPtrFactory<SettingsWatcher> weak_ptr_factory_{this};
  };

  // Struct which represents the PageInfo state of interest to the service.
  struct PageInfoState {
    PageInfoState();
    base::Time opened_time;
    bool interacted = false;
  };

  void SettingsWatcherComplete();

  // Record that a trigger occurred, placing it in the set of pending triggers.
  // Private as the service itself determines when a trigger has occurred, and
  // is responsible for generating the appropriate |product_specific_data|.
  void TriggerOccurred(
      FeatureArea feature_area,
      const std::map<std::string, bool>& product_specific_data);

  // Record that the user performed an action which should make them temporarily
  // ineligible to receive a survey. This records a trigger for the kIneligible
  // feature area, which just like any other trigger will prevent a survey from
  // being shown, but will not result in a survey.
  void PerformedIneligibleAction();

  static bool ShouldBlockSurvey(const PendingTrigger& trigger);

  const raw_ptr<Profile> profile_;
  std::map<FeatureArea, PendingTrigger> pending_triggers_;
  std::unique_ptr<SettingsWatcher> settings_watcher_;
  std::unique_ptr<PageInfoState> page_info_state_;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  bool performed_control_group_dice_roll_;
  base::WeakPtrFactory<TrustSafetySentimentService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
