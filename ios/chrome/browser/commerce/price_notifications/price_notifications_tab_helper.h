// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COMMERCE_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TAB_HELPER_H_

#import "base/scoped_observation.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol PriceNotificationsIPHPresenter;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// The PriceNotificationTabHelper's purpose is to initiate display of the Price
// Tracking IPH when the user navigates to a webpage that contains an item that
// can be price tracked.
class PriceNotificationsTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<PriceNotificationsTabHelper> {
 public:
  PriceNotificationsTabHelper(const PriceNotificationsTabHelper&) = delete;
  PriceNotificationsTabHelper& operator=(const PriceNotificationsTabHelper&) =
      delete;

  ~PriceNotificationsTabHelper() override;

  // Sets the presenter for follow in-product help (IPH). `presenter` is not
  // retained by this tab helper.
  void SetPriceNotificationsIPHPresenter(
      id<PriceNotificationsIPHPresenter> presenter) {
    price_notifications_iph_presenter_ = presenter;
  }

 private:
  friend class web::WebStateUserData<PriceNotificationsTabHelper>;

  explicit PriceNotificationsTabHelper(web::WebState* web_state);

  // WebStateObserver::
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // Manages the tab helper's observation of the WebState
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // The service responsible for determining whether a given webpage can be
  // price tracked.
  commerce::ShoppingService* shopping_service_ = nullptr;

  // The presenter that displays the price tracking bubble IPH.
  __weak id<PriceNotificationsIPHPresenter> price_notifications_iph_presenter_ =
      nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};
#endif  // IOS_CHROME_BROWSER_COMMERCE_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TAB_HELPER_H_
