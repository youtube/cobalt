// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_view_controller_presentation_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kURLUsageBannerName = @"url_usage_illustration";

enum SectionIdentifier {
  kSectionIdentifierSwitch,
  kSectionIdentifierWhenOn,
  kSectionIdentifierThingsToConsider,
};

enum ItemIdentifier {
  kItemIdentifierSwitch,
  kItemIdentifierBrowseFaster,
  kItemIdentifierImprovedSuggestions,
  kItemIdentifierPredictSites,
  kItemIdentifierUsageReport,
};

}  // namespace

@interface PrivacyGuideURLUsageViewController () <UITableViewDelegate>
@end

@implementation PrivacyGuideURLUsageViewController {
  SelfSizingTableView* _tableView;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  BOOL _URLUsageEnabled;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kPrivacyGuideURLUsageViewID;

  self.bannerName = kURLUsageBannerName;
  self.bannerSize = BannerImageSizeType::kShort;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_NEXT_BUTTON);

  self.subtitleBottomMargin = 0;

  [super viewDidLoad];

  [self setupTableView];
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    [self.presentationDelegate privacyGuideViewControllerDidRemove:self];
  }
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return section == kSectionIdentifierSwitch ? 0
                                             : UITableViewAutomaticDimension;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case kSectionIdentifierSwitch:
      return nil;
    case kSectionIdentifierWhenOn: {
      return PrivacyGuideHeaderView(_tableView,
                                    IDS_IOS_PRIVACY_GUIDE_WHEN_ON_HEADER);
    }
    case kSectionIdentifierThingsToConsider: {
      return PrivacyGuideHeaderView(
          _tableView, IDS_IOS_PRIVACY_GUIDE_THINGS_TO_CONSIDER_HEADER);
    }
  }
}

#pragma mark - PrivacyGuideURLUsageConsumer

- (void)setURLUsageEnabled:(BOOL)enabled {
  _URLUsageEnabled = enabled;

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ @(kItemIdentifierSwitch) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - Private

// Initializes a SelfSizingTableView, adds it to the view hierarchy and
// specifies its constraints.
- (void)setupTableView {
  _tableView = PrivacyGuideTableView();
  _tableView.delegate = self;

  [self.specificContentView addSubview:_tableView];
  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [_tableView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
    [_tableView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
}

// Initializes the data source and populates the initial snapshot.
- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewSwitchCell>(_tableView);
  RegisterTableViewCell<SettingsImageDetailTextCell>(_tableView);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(_tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[ @(kSectionIdentifierSwitch) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kItemIdentifierSwitch) ]];

  [snapshot appendSectionsWithIdentifiers:@[ @(kSectionIdentifierWhenOn) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(kItemIdentifierBrowseFaster), @(kItemIdentifierImprovedSuggestions)
  ]];

  [snapshot
      appendSectionsWithIdentifiers:@[ @(kSectionIdentifierThingsToConsider) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(kItemIdentifierPredictSites), @(kItemIdentifierUsageReport)
  ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the appropriate cell for the table view.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kItemIdentifierSwitch: {
      TableViewSwitchCell* cell = PrivacyGuideSwitchCell(
          _tableView,
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT, YES,
          _URLUsageEnabled, kPrivacyGuideURLUsageSwitchID);
      [cell.switchView addTarget:self
                          action:@selector(URLUsageSwitchChanged:)
                forControlEvents:UIControlEventValueChanged];
      return cell;
    }
    case kItemIdentifierBrowseFaster: {
      return PrivacyGuideExplanationCell(
          _tableView, IDS_IOS_PRIVACY_GUIDE_URL_USAGE_BROWSER_FASTER,
          kBoltSymbol);
    }
    case kItemIdentifierImprovedSuggestions: {
      return PrivacyGuideExplanationCell(
          _tableView, IDS_IOS_PRIVACY_GUIDE_URL_USAGE_IMPROVED_SUGGESTIONS,
          kLightBulbSymbol);
    }
    case kItemIdentifierPredictSites: {
      return PrivacyGuideExplanationCell(
          _tableView, IDS_IOS_PRIVACY_GUIDE_URL_USAGE_PREDICT_SITES,
          kLinkActionSymbol);
    }
    case kItemIdentifierUsageReport: {
      return PrivacyGuideExplanationCell(
          _tableView, IDS_IOS_PRIVACY_GUIDE_URL_USAGE_USAGE_REPORT,
          kShareSymbol);
    }
  }
}

// Called when the switch is tapped.
- (void)URLUsageSwitchChanged:(UISwitch*)sender {
  [self.modelDelegate didEnableURLUsage:sender.isOn];
}

@end
