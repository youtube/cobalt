// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_

#include <map>

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/infobars/badge_state.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

@protocol InfobarBadgeTabHelperDelegate;
@protocol LegacyInfobarBadgeTabHelperDelegate;

// TabHelper that observes InfoBarManager. It updates an InfobarBadge delegate
// for relevant Infobar changes.
class InfobarBadgeTabHelper
    : public web::WebStateUserData<InfobarBadgeTabHelper> {
 public:
  InfobarBadgeTabHelper(const InfobarBadgeTabHelper&) = delete;
  InfobarBadgeTabHelper& operator=(const InfobarBadgeTabHelper&) = delete;

  ~InfobarBadgeTabHelper() override;

  // Sets the InfobarBadgeTabHelperDelegate to `delegate`.
  void SetDelegate(id<InfobarBadgeTabHelperDelegate> delegate);
  // Updates Infobar for the case where the user is aware that they could access
  // the infobar with `infobar_type` through a badge.
  void UpdateBadgeForInfobarRead(InfobarType infobar_type);
  // Updates Infobar for the case where an Infobar banner of
  // `infobar_type` was presented.
  void UpdateBadgeForInfobarBannerPresented(InfobarType infobar_type);
  // Updates Infobar for the case where an Infobar banner of
  // `infobar_type` was dismissed.
  void UpdateBadgeForInfobarBannerDismissed(InfobarType infobar_type);

  // DEPRECATED: The accept state of an infobar is now stored directly in
  // InfoBarIOS, and should be updated there rather than using these functions.
  void UpdateBadgeForInfobarAccepted(InfobarType infobar_type);
  void UpdateBadgeForInfobarReverted(InfobarType infobar_type);

  // Returns all BadgesStates for infobars.
  std::map<InfobarType, BadgeState> GetInfobarBadgeStates() const;

 private:
  friend class web::WebStateUserData<InfobarBadgeTabHelper>;
  explicit InfobarBadgeTabHelper(web::WebState* web_state);

  // Registers/unregisters the infobar to the tab helper for observation of its
  // activities.
  void RegisterInfobar(infobars::InfoBar* infobar);
  void UnregisterInfobar(infobars::InfoBar* infobar);
  // Notifies the tab helper that an infobar with `type` was accepted or
  // reverted.
  void OnInfobarAcceptanceStateChanged(InfobarType infobar_type, bool accepted);
  // Update the badges displayed in the location bar.
  void UpdateBadgesShown();

  // Helper object that listens for accept and revert events for an InfoBarIOS.
  class InfobarAcceptanceObserver : public InfoBarIOS::Observer {
   public:
    explicit InfobarAcceptanceObserver(InfobarBadgeTabHelper* tab_helper);
    ~InfobarAcceptanceObserver() override;

    // Returns a reference to the scoped observations.
    base::ScopedMultiSourceObservation<InfoBarIOS, InfoBarIOS::Observer>&
    scoped_observations() {
      return scoped_observations_;
    }

   private:
    // InfoBarIOS::Observer:
    void DidUpdateAcceptedState(InfoBarIOS* infobar) override;
    void InfobarDestroyed(InfoBarIOS* infobar) override;

    // The owning tab helper.
    InfobarBadgeTabHelper* tab_helper_ = nullptr;
    // Scoped observer that facilitates observing InfoBarIOS objects.
    base::ScopedMultiSourceObservation<InfoBarIOS, InfoBarIOS::Observer>
        scoped_observations_{this};
  };

  // Helper object that updates state and adds an InfobarAcceptanceObserver
  // when each infobar is added or removed.
  class InfobarManagerObserver : public infobars::InfoBarManager::Observer {
   public:
    InfobarManagerObserver(InfobarBadgeTabHelper* tab_helper,
                           web::WebState* web_state,
                           InfobarAcceptanceObserver* infobar_accept_observer);
    ~InfobarManagerObserver() override;

   private:
    // InfoBarManagerObserver:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

    // The owning tab helper.
    InfobarBadgeTabHelper* tab_helper_ = nullptr;
    // The infobar acceptance observer for `tab_helper_`.  Added to each infobar
    // in the observed manager.
    InfobarAcceptanceObserver* infobar_accept_observer_ = nullptr;
    // Scoped observer that facilitates observing an InfoBarManager.
    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        scoped_observation_{this};
  };

  // Delegate which displays the Infobar badge.
  __weak id<InfobarBadgeTabHelperDelegate> delegate_ = nil;
  // The infobar accept/revert observer.
  InfobarAcceptanceObserver infobar_accept_observer_;
  // The infobar manager observer.
  InfobarManagerObserver infobar_manager_observer_;
  // The WebState this TabHelper is scoped to.
  web::WebState* web_state_;
  // Map storing the BadgeState for each InfobarType.
  std::map<InfobarType, BadgeState> infobar_badge_states_;
  // Vector storing infobars that are added when prerendering.
  std::vector<infobars::InfoBar*> infobars_added_when_prerendering_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
