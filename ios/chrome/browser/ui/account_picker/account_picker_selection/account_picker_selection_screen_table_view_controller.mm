// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_table_view_controller_model_delegate.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  IdentitySectionIdentifier = kSectionIdentifierEnumZero,
  AddAccountSectionIdentifier,
};

typedef NS_ENUM(NSInteger, ItemType) {
  // IdentitySectionIdentifier section.
  IdentityItemType = kItemTypeEnumZero,
  // Indication that some restricted accounts were removed from the list.
  ItemTypeRestrictedAccountsFooter,
  // AddAccountSectionIdentifier section.
  AddAccountItemType,
};

// Table view header/footer height.
CGFloat kSectionHeaderHeight = 8.;
CGFloat kSectionFooterHeight = 8.;

}  // namespace

@interface AccountPickerSelectionScreenTableViewController () <
    TableViewLinkHeaderFooterItemDelegate>

@end

@implementation AccountPickerSelectionScreenTableViewController

#pragma mark - UIView

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  [self.tableView reloadData];
  self.view.backgroundColor = UIColor.clearColor;
}

#pragma mark - UITableViewController

- (void)loadModel {
  [super loadModel];
  [self loadIdentitySection];
  [self loadAddAccountSection];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch ((ItemType)item.type) {
    case IdentityItemType: {
      TableViewIdentityItem* identityItem =
          base::apple::ObjCCastStrict<TableViewIdentityItem>(item);
      DCHECK(identityItem);
      [self.actionDelegate
          accountPickerListTableViewController:self
                   didSelectIdentityWithGaiaID:identityItem.gaiaID];
      break;
    }
    case AddAccountItemType:
      [self.actionDelegate
          accountPickerListTableViewControllerDidTapOnAddAccount:self];
      break;
    case ItemTypeRestrictedAccountsFooter:
      NOTREACHED();
      break;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kSectionHeaderHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - Model Items

- (TableViewLinkHeaderFooterItem*)restrictedIdentitiesFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeRestrictedAccountsFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_RESTRICTED_IDENTITIES);
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kChromeUIManagementURL)] ];
  return footer;
}

#pragma mark - Private

// Creates the identity section in the table view model.
- (void)loadIdentitySection {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:IdentitySectionIdentifier];
  [self loadIdentityItems];

  if (IsRestrictAccountsToPatternsEnabled()) {
    [model setFooter:[self restrictedIdentitiesFooterItem]
        forSectionWithIdentifier:IdentitySectionIdentifier];
  }
}

// Creates all the identity items in the table view model.
- (void)loadIdentityItems {
  TableViewModel* model = self.tableViewModel;
  for (AccountPickerSelectionScreenIdentityItemConfigurator* configurator in
           self.modelDelegate.sortedIdentityItemConfigurators) {
    TableViewIdentityItem* item =
        [[TableViewIdentityItem alloc] initWithType:IdentityItemType];
    item.identityViewStyle = IdentityViewStyleConsistency;
    [configurator configureIdentityChooser:item];
    [model addItem:item toSectionWithIdentifier:IdentitySectionIdentifier];
  }
}

// Creates the add account section in the table view model.
- (void)loadAddAccountSection {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:AddAccountSectionIdentifier];
  TableViewImageItem* item =
      [[TableViewImageItem alloc] initWithType:AddAccountItemType];
  item.title = l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_ADD_ACCOUNT);
  item.textColor = [UIColor colorNamed:kBlueColor];
  [model addItem:item toSectionWithIdentifier:AddAccountSectionIdentifier];
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case IdentitySectionIdentifier: {
      TableViewLinkHeaderFooterView* linkView =
          base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
    case AddAccountSectionIdentifier:
      break;
  }
  return view;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  DCHECK(URL.gurl == GURL(kChromeUIManagementURL));
  DCHECK(self.actionDelegate);
  [self.actionDelegate showManagementHelpPage];
}

#pragma mark - AccountPickerSelectionScreenConsumer

- (void)reloadAllIdentities {
  TableViewModel* model = self.tableViewModel;
  [model deleteAllItemsFromSectionWithIdentifier:IdentitySectionIdentifier];
  [self loadIdentityItems];

  NSUInteger sectionIndex =
      [model sectionForSectionIdentifier:IdentitySectionIdentifier];
  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
  [self.tableView reloadSections:indexSet
                withRowAnimation:UITableViewRowAnimationFade];
}

- (void)reloadIdentityForIdentityItemConfigurator:
    (AccountPickerSelectionScreenIdentityItemConfigurator*)configurator {
  TableViewModel* model = self.tableViewModel;
  NSInteger section =
      [model sectionForSectionIdentifier:IdentitySectionIdentifier];
  NSInteger itemCount = [model numberOfItemsInSection:section];
  for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                            inSection:section];
    TableViewIdentityItem* item =
        base::apple::ObjCCastStrict<TableViewIdentityItem>(
            [model itemAtIndexPath:path]);
    if ([item.gaiaID isEqualToString:configurator.gaiaID]) {
      [configurator configureIdentityChooser:item];
      [self reconfigureCellsForItems:@[ item ]];
      [self.tableView reloadRowsAtIndexPaths:@[ path ]
                            withRowAnimation:UITableViewRowAnimationNone];
      break;
    }
  }
}

@end
