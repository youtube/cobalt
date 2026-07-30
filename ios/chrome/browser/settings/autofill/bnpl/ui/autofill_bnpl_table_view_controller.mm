// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/bnpl/ui/autofill_bnpl_table_view_controller.h"

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBnplSwitch = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeBnplSwitch = kItemTypeEnumZero,
  ItemTypeBnplSwitchSubtitle,
};

}  // namespace

@implementation AutofillBnplTableViewController

@synthesize bnplSwitchIsOn = _bnplSwitchIsOn;

- (void)setBnplSwitchIsOn:(BOOL)bnplSwitchIsOn {
  if (_bnplSwitchIsOn == bnplSwitchIsOn) {
    return;
  }
  _bnplSwitchIsOn = bnplSwitchIsOn;
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_PAY_OVER_TIME);
    self.shouldDisableDoneButtonOnEdit = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAutofillBnplTableViewId;
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierBnplSwitch];
  [model addItem:[self bnplSwitchItem]
      toSectionWithIdentifier:SectionIdentifierBnplSwitch];
  [model setFooter:[self bnplSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierBnplSwitch];
}

#pragma mark - LoadModel Helpers

- (TableViewItem*)bnplSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeBnplSwitch];
  switchItem.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PAY_OVER_TIME);
  switchItem.target = self;
  switchItem.selector = @selector(bnplSwitchChanged:);
  switchItem.on = self.bnplSwitchIsOn;
  switchItem.accessibilityIdentifier = kAutofillBnplSwitchViewId;
  return switchItem;
}

- (TableViewHeaderFooterItem*)bnplSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeBnplSwitchSubtitle];
  footer.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PAY_OVER_TIME_SUBTITLE);
  return footer;
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return NO;
}

#pragma mark - Switch Callbacks

- (void)bnplSwitchChanged:(UISwitch*)switchView {
  [self.delegate viewController:self didChangeBnplSwitchTo:[switchView isOn]];
}

@end
