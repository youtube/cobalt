// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_metrics.h"

#include <string>

#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_metrics {

namespace {

const AccessPoint kAccessPointsThatSupportUserAction[] = {
    AccessPoint::ACCESS_POINT_START_PAGE,
    AccessPoint::ACCESS_POINT_NTP_LINK,
    AccessPoint::ACCESS_POINT_MENU,
    AccessPoint::ACCESS_POINT_SETTINGS,
    AccessPoint::ACCESS_POINT_SUPERVISED_USER,
    AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
    AccessPoint::ACCESS_POINT_EXTENSIONS,
    AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
    AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
    AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
    AccessPoint::ACCESS_POINT_USER_MANAGER,
    AccessPoint::ACCESS_POINT_DEVICES_PAGE,
    AccessPoint::ACCESS_POINT_CLOUD_PRINT,
    AccessPoint::ACCESS_POINT_CONTENT_AREA,
    AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
    AccessPoint::ACCESS_POINT_RECENT_TABS,
    AccessPoint::ACCESS_POINT_UNKNOWN,
    AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
    AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN,
    AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS,
    AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR,
    AccessPoint::ACCESS_POINT_TAB_SWITCHER,
    AccessPoint::ACCESS_POINT_MACHINE_LOGON,
    AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS,
    AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO,
    AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO,
    AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO,
    AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO,
    AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW,
    AccessPoint::ACCESS_POINT_READING_LIST,
    AccessPoint::ACCESS_POINT_SET_UP_LIST,
};

const AccessPoint kAccessPointsThatSupportImpression[] = {
    AccessPoint::ACCESS_POINT_START_PAGE,
    AccessPoint::ACCESS_POINT_NTP_LINK,
    AccessPoint::ACCESS_POINT_MENU,
    AccessPoint::ACCESS_POINT_SETTINGS,
    AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
    AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
    AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
    AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
    AccessPoint::ACCESS_POINT_DEVICES_PAGE,
    AccessPoint::ACCESS_POINT_CLOUD_PRINT,
    AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
    AccessPoint::ACCESS_POINT_RECENT_TABS,
    AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
    AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN,
    AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS,
    AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR,
    AccessPoint::ACCESS_POINT_TAB_SWITCHER,
    AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO,
    AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO,
    AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO,
    AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO,
    AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW,
    AccessPoint::ACCESS_POINT_READING_LIST,
    AccessPoint::ACCESS_POINT_SET_UP_LIST,
};

class SigninMetricsTest : public ::testing::Test {
 public:
  static std::string GetAccessPointDescription(AccessPoint access_point) {
    switch (access_point) {
      case AccessPoint::ACCESS_POINT_START_PAGE:
        return "StartPage";
      case AccessPoint::ACCESS_POINT_NTP_LINK:
        return "NTP";
      case AccessPoint::ACCESS_POINT_MENU:
        return "Menu";
      case AccessPoint::ACCESS_POINT_SETTINGS:
        return "Settings";
      case AccessPoint::ACCESS_POINT_SUPERVISED_USER:
        return "SupervisedUser";
      case AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
        return "ExtensionInstallBubble";
      case AccessPoint::ACCESS_POINT_EXTENSIONS:
        return "Extensions";
      case AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
        return "BookmarkBubble";
      case AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
        return "BookmarkManager";
      case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
        return "AvatarBubbleSignin";
      case AccessPoint::ACCESS_POINT_USER_MANAGER:
        return "UserManager";
      case AccessPoint::ACCESS_POINT_DEVICES_PAGE:
        return "DevicesPage";
      case AccessPoint::ACCESS_POINT_CLOUD_PRINT:
        return "CloudPrint";
      case AccessPoint::ACCESS_POINT_CONTENT_AREA:
        return "ContentArea";
      case AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
        return "SigninPromo";
      case AccessPoint::ACCESS_POINT_RECENT_TABS:
        return "RecentTabs";
      case AccessPoint::ACCESS_POINT_UNKNOWN:
        return "UnknownAccessPoint";
      case AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
        return "PasswordBubble";
      case AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
        return "AutofillDropdown";
      case AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
        return "NTPContentSuggestions";
      case AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
        return "ReSigninInfobar";
      case AccessPoint::ACCESS_POINT_TAB_SWITCHER:
        return "TabSwitcher";
      case AccessPoint::ACCESS_POINT_MACHINE_LOGON:
        return "MachineLogon";
      case AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
        return "GoogleServicesSettings";
      case AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
        return "SyncErrorCard";
      case AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
        return "ForcedSignin";
      case AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
        return "AccountRenamed";
      case AccessPoint::ACCESS_POINT_WEB_SIGNIN:
        return "WebSignIn";
      case AccessPoint::ACCESS_POINT_SAFETY_CHECK:
        return "SafetyCheck";
      case AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
        return "Kaleidoscope";
      case AccessPoint::ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
        return "EnterpriseSignoutResignSheet";
      case AccessPoint::ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
        return "SigninInterceptFirstRunExperience";
      case AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
        return "SendTabToSelfPromo";
      case AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
        return "NTPFeedTopPromo";
      case AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
        return "SettingsSyncOffRow";
      case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
        return "PostDeviceRestoreSigninPromo";
      case AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
        return "PostDeviceRestoreBackgroundSignin";
      case AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
        return "NTPSignedOutIcon";
      case AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
        return "NTPFeedCardMenuSigninPromo";
      case AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
        return "NTPFeedBottomSigninPromo";
      case AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
        return "DesktopSigninManager";
      case AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
        return "ForYouFre";
      case AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
        return "CreatorFeedFollow";
      case AccessPoint::ACCESS_POINT_READING_LIST:
        return "ReadingList";
      case AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
        return "ReauthInfoBar";
      case AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
        return "AccountConsistencyService";
      case AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
        return "SearchCompanion";
      case AccessPoint::ACCESS_POINT_SET_UP_LIST:
        return "SetUpList";
      case AccessPoint::ACCESS_POINT_MAX:
        NOTREACHED();
        return "";
    }
    NOTREACHED();
    return "";
  }
};

TEST_F(SigninMetricsTest, RecordSigninUserActionForAccessPoint) {
  for (const AccessPoint& ap : kAccessPointsThatSupportUserAction) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(ap);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_From" + GetAccessPointDescription(ap)));
  }
}

TEST_F(SigninMetricsTest, RecordSigninImpressionUserAction) {
  for (const AccessPoint& ap : kAccessPointsThatSupportImpression) {
    base::UserActionTester user_action_tester;
    RecordSigninImpressionUserActionForAccessPoint(ap);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Impression_From" + GetAccessPointDescription(ap)));
  }
}

}  // namespace
}  // namespace signin_metrics
