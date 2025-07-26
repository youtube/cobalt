// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kOsToggleIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsHidden);

auto ElementIsVisibleStateChange(
    ui::CustomElementEventType event,
    WebContentsInteractionTestUtil::DeepQuery query) {
  WebContentsInteractionTestUtil::StateChange element_is_visible;
  element_is_visible.event = event;
  element_is_visible.where = query;
  element_is_visible.type =
      WebContentsInteractionTestUtil::StateChange::Type::kExists;
  return element_is_visible;
}

auto ElementIsHiddenStateChange(
    ui::CustomElementEventType event,
    WebContentsInteractionTestUtil::DeepQuery query) {
  WebContentsInteractionTestUtil::StateChange element_is_hidden;
  element_is_hidden.event = event;
  element_is_hidden.where = query;
  element_is_hidden.type =
      WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist;
  return element_is_hidden;
}
}  // namespace

class GlicSettingsUtilUiTest
    : public glic::test::InteractiveGlicFeaturePromoTest {
 public:
  GlicSettingsUtilUiTest() = default;
  ~GlicSettingsUtilUiTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicFeaturePromoTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, true);
  }

  auto VerifyOpensGlicSettings(auto f) {
    return Steps(
        Do([this, f]() { f(browser()->profile()); }),
        InstrumentTab(kSettingsTab),
        WaitForWebContentsReady(
            kSettingsTab, chrome::GetSettingsUrl(chrome::kChromeUIGlicHost)));
  }

  const DeepQuery kOsToggleHelpBubbleQuery{"settings-ui",
                                           "settings-main",
                                           "settings-basic-page",
                                           "settings-glic-page",
                                           "#launcherToggle",
                                           "help-bubble",
                                           "#close"};

  const DeepQuery kKeyboardShortcutHelpBubbleQuery{
      "settings-ui",        "settings-main", "settings-basic-page",
      "settings-glic-page", "help-bubble",   "#close"};
};

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenSettings) {
  RunTestSequence(VerifyOpensGlicSettings(glic::OpenGlicSettingsPage));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenOsToggleSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(
          kSettingsTab, ElementIsVisibleStateChange(kBubbleIsVisible,
                                                    kOsToggleHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenKeyboardShortcutSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicKeyboardShortcutSetting),
      WaitForStateChange(kSettingsTab, ElementIsVisibleStateChange(
                                           kBubbleIsVisible,
                                           kKeyboardShortcutHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, ThrottleOpenOsToggleSetting) {
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(browser()->profile(),
                                                       features::kGlic);
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(
          kSettingsTab,
          ElementIsVisibleStateChange(
              kOsToggleIsVisible,
              {"settings-ui", "settings-main", "settings-basic-page",
               "settings-glic-page", "#launcherToggle"})),
      WaitForStateChange(kSettingsTab,
                         ElementIsHiddenStateChange(kBubbleIsHidden,
                                                    kOsToggleHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest,
                       ThrottleOpenKeyboardShortcutSetting) {
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      browser()->profile(), features::kGlicKeyboardShortcutNewBadge);
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(
          kSettingsTab,
          ElementIsVisibleStateChange(
              kOsToggleIsVisible,
              {"settings-ui", "settings-main", "settings-basic-page",
               "settings-glic-page", "#shortcutInput"})),
      WaitForStateChange(kSettingsTab, ElementIsHiddenStateChange(
                                           kBubbleIsHidden,
                                           kKeyboardShortcutHelpBubbleQuery)));
}
