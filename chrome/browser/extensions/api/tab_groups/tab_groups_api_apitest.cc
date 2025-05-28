// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

namespace {

using TabGroupsApiTest = ExtensionApiTest;

// TODO(crbug.com/40910190): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupsWorks) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/basics")) << message_;
}

// Tests that events are restricted to their respective browser contexts,
// especially between on-the-record and off-the-record browsers.
// Note: unit tests don't support multiple profiles, so this has to be a browser
// test.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupEventsAcrossProfiles) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  // The EventRouter is shared between on- and off-the-record profiles, so
  // this observer will catch events for each. To verify that the events are
  // restricted to their respective contexts, we check the event metadata.
  TestEventRouterObserver event_observer(
      EventRouter::Get(browser()->profile()));

  browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* normal_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(normal_event->restrict_to_browser_context, browser()->profile());

  event_observer.ClearEvents();

  incognito_browser->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* incognito_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(incognito_event->restrict_to_browser_context,
            incognito_browser->profile());
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, SetGroupTitleToEmoji) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/emoji",
                               {.extension_url = "emoji_title.html"}))
      << message_;

  std::optional<tab_groups::TabGroupId> group =
      browser()->tab_strip_model()->GetTabGroupForTab(0);
  ASSERT_TRUE(group.has_value());
  const tab_groups::TabGroupVisualData* visual_data = browser()
                                                          ->tab_strip_model()
                                                          ->group_model()
                                                          ->GetTabGroup(*group)
                                                          ->visual_data();
  EXPECT_EQ(visual_data->title(), std::u16string(u"🤡"));
}

}  // namespace
}  // namespace extensions
