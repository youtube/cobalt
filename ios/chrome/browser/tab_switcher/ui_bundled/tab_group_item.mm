// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

@implementation TabGroupItem {
  base::WeakPtr<const TabGroup> _tabGroup;
  raw_ptr<const void, DanglingUntriaged> _tabGroupIdentifier;
  TabGroupColorPalette* _tabGroupColorPalette;
  tab_groups::TabGroupColorId _colorId;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup {
  CHECK(tabGroup);
  self = [super init];
  if (self) {
    _tabGroup = tabGroup->GetWeakPtr();
    _tabGroupIdentifier = tabGroup;
  }
  return self;
}

- (const void*)tabGroupIdentifier {
  return _tabGroupIdentifier;
}

- (const TabGroup*)tabGroup {
  return _tabGroup.get();
}

- (NSString*)title {
  if (!_tabGroup) {
    return nil;
  }
  return _tabGroup->GetTitle();
}

- (TabGroupColorPalette*)tabGroupColorPalette {
  if (!_tabGroup) {
    return nil;
  }
  tab_groups::TabGroupColorId currentColorId = _tabGroup->GetColor();
  if (!_tabGroupColorPalette || _colorId != currentColorId) {
    _colorId = currentColorId;
    _tabGroupColorPalette =
        [[TabGroupColorPalette alloc] initWithColorId:_colorId];
  }
  return _tabGroupColorPalette;
}

- (UIColor*)foregroundColor {
  if (!_tabGroup) {
    return nil;
  }
  return tab_groups::ForegroundColorForTabGroupColorId(_tabGroup->GetColor());
}

- (NSInteger)numberOfTabsInGroup {
  if (!_tabGroup) {
    return 0;
  }
  return _tabGroup->range().count();
}

- (BOOL)collapsed {
  if (!_tabGroup) {
    return NO;
  }
  return _tabGroup->visual_data().is_collapsed();
}

- (UIColor*)tabStripColor {
  return self.tabGroupColorPalette.commonColor;
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

@end
