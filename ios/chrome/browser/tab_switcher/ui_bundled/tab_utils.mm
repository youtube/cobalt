// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"

#import <set>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/web_state.h"

TabItem* GetTabItem(WebStateList* web_state_list,
                    WebStateSearchCriteria criteria) {
  web::WebState* web_state = GetWebState(web_state_list, criteria);
  if (!web_state) {
    return nil;
  }

  TabItem* item =
      [[TabItem alloc] initWithTitle:tab_util::GetTabTitle(web_state)
                                 URL:web_state->GetVisibleURL()];
  return item;
}

bool HasDuplicateGroupsAndTabsIdentifiers(NSArray<GridItemIdentifier*>* items) {
  std::set<web::WebStateID> identifiers;
  std::set<const TabGroup*> groups;
  for (GridItemIdentifier* item in items) {
    switch (item.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab:
        identifiers.insert(item.tabSwitcherItem.identifier);
        break;
      case GridItemType::kGroup:
        groups.insert(item.tabGroupItem.tabGroup);
        break;
      case GridItemType::kSuggestedActions:
        NOTREACHED();
      case GridItemType::kActivitySummary:
        NOTREACHED();
    }
  }
  return (identifiers.size() + groups.size()) != items.count;
}

bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items) {
  std::set<web::WebStateID> identifiers;
  for (TabSwitcherItem* item in items) {
    identifiers.insert(item.identifier);
  }
  return identifiers.size() != items.count;
}

Browser* GetBrowserForTabWithCriteria(BrowserList* browser_list,
                                      WebStateSearchCriteria criteria,
                                      bool is_otr_tab) {
  const BrowserList::BrowserType browser_types =
      is_otr_tab ? BrowserList::BrowserType::kIncognito
                 : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    int index = GetWebStateIndex(web_state_list, criteria);
    if (index != WebStateList::kInvalidIndex) {
      return browser;
    }
  }
  return nullptr;
}
