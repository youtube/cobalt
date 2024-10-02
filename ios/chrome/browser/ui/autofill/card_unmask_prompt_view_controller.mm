// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller+private.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_header_item.h"
#import "ios/chrome/browser/ui/autofill/cells/expiration_date_edit_item.h"
#import "ios/chrome/browser/ui/autofill/cells/expiration_date_edit_item_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCardUnmaskPromptTableViewAccessibilityID =
    @"CardUnmaskPromptTableViewAccessibilityID";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierInputs,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeCVCInput,
  ItemTypeFooter,
  ItemTypeExpirationDateInput,
};

// Empty space on top of the input section. This value added up to the gPay
// badge bottom padding achieves the mock's vertical spacing between the gPay
// badge in the header.
const CGFloat kInputsSectionTopSpacing = 18;
// When the inputs section doesn't display a footer, an empty one is displayed
// with this height to provide spacing to the bottom of the tableView.
const CGFloat kEmptyFooterHeight = 10;
// Estimated height of the header/footer, used to speed the constraints.
const CGFloat kEstimatedHeaderFooterHeight = 50;
// Dummy URL used as target of the link in the footer.
const char kFooterDummyLinkTarget[] = "about:blank";

}  // namespace

@interface CardUnmaskPromptViewController () <
    ExpirationDateEditItemDelegate,
    TableViewLinkHeaderFooterItemDelegate,
    TableViewTextEditItemDelegate,
    UITextFieldDelegate> {
  // Button displayed on the right side of the navigation bar.
  // Tapping it sends the data in the prompt for verification.
  UIBarButtonItem* _confirmButton;
  // Owns `self`. A value of nullptr means the view controller is dismissed or
  // about to be dismissed.
  autofill::CardUnmaskPromptViewBridge* _bridge;  // weak
  // Model of the CVC input cell.
  TableViewTextEditItem* _CVCInputItem;
  // Model of the footer.
  TableViewLinkHeaderFooterItem* _footerItem;
  // Model of the header.
  CVCHeaderItem* _headerItem;
  // Model of the expiration date input cell.
  ExpirationDateEditItem* _expirationDateInputItem;
  // Flag indicating that we should set the focus in either the CVC or
  // expiration date fields once the tableView is reloaded. After the view shows
  // the CVC form or the expiration form, we want to focus the CVC and
  // expiration date fields respectively. When transitioning from one form to
  // the other, we activate this flag so the focus is updated.
  BOOL _shouldUpdateFocus;
}

@end

@implementation CardUnmaskPromptViewController

- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _bridge = bridge;
    // Focus CVC field after initial load.
    _shouldUpdateFocus = YES;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (_bridge == nullptr) {
    return;
  }

  // Default inset inherited from `super` is for cells that display icons.
  // Using smaller inset.
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);

  self.tableView.accessibilityIdentifier =
      kCardUnmaskPromptTableViewAccessibilityID;

  self.title =
      base::SysUTF16ToNSString(_bridge->GetController()->GetWindowTitle());

  // Disable selection.
  self.tableView.allowsSelection = NO;

  self.tableView.estimatedSectionFooterHeight = kEstimatedHeaderFooterHeight;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedHeaderFooterHeight;

  self.navigationItem.leftBarButtonItem = [self createCancelButton];

  _confirmButton = [self createConfirmButton];
  // Disable confirm button until valid input is entered.
  _confirmButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = _confirmButton;

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Provide context for users with Voice Over enabled.
  NSString* initialMessage =
      [NSString stringWithFormat:@"%@\n%@", self.title, [self instructions]];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  initialMessage);
}

- (void)loadModel {
  [super loadModel];

  if (_bridge == nullptr) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];

  _headerItem = [self createHeaderItem];
  [self updateInstructions];
  [model setHeader:_headerItem
      forSectionWithIdentifier:SectionIdentifierHeader];

  [model addSectionWithIdentifier:SectionIdentifierInputs];

  _CVCInputItem = [self createCVCInputItem];
  [self.tableViewModel addItem:_CVCInputItem
       toSectionWithIdentifier:SectionIdentifierInputs];
}

#pragma mark - Public

- (void)showLoadingState {
  self.tableView.userInteractionEnabled = NO;

  UIActivityIndicatorView* activityIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  UIBarButtonItem* barButton =
      [[UIBarButtonItem alloc] initWithCustomView:activityIndicator];
  self.navigationItem.rightBarButtonItem = barButton;
  [activityIndicator startAnimating];
}

- (void)showErrorAlertWithMessage:(NSString*)message
                   closeOnDismiss:(BOOL)closeOnDismiss {
  // Restore confirm button to navigation bar.
  self.navigationItem.rightBarButtonItem = _confirmButton;

  UIAlertController* errorAlert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_ALERT_TITLE)
                       message:message
                preferredStyle:UIAlertControllerStyleAlert];

  auto* __weak weakSelf = self;
  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_OK)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                [weakSelf onErrorAlertDismissedAndShouldCloseOnDismiss:
                              closeOnDismiss];
              }];

  [errorAlert addAction:okAction];

  [self presentViewController:errorAlert animated:YES completion:nil];
}

- (void)disconnectFromBridge {
  _bridge = nullptr;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (_bridge == nullptr) {
    return;
  }
  // Notify bridge that UI was dismissed.
  _bridge->NavigationControllerDismissed();
}

#pragma mark - Actions

- (void)onCancelTapped {
  if (_bridge) {
    _bridge->PerformClose();
  }
}

- (void)onVerifyTapped {
  if (_bridge == nullptr) {
    return;
  }

  autofill::CardUnmaskPromptController* controller = _bridge->GetController();

  NSString* CVC = _CVCInputItem.textFieldValue;
  NSString* month = nil;
  NSString* year = nil;

  if (_expirationDateInputItem) {
    month = _expirationDateInputItem.month;
    year = _expirationDateInputItem.year;
  }

  controller->OnUnmaskPromptAccepted(
      base::SysNSStringToUTF16(CVC), base::SysNSStringToUTF16(month),
      base::SysNSStringToUTF16(year), /*enable_fido_auth=*/false);
}

#pragma mark - Private

// Displays the form for updating the card's expiration date.
// Displayed on this state:
//   - Header with instructions label and gPay badge.
//   - CVC input field.
//   - Update expiration date input field.
- (void)showUpdateExpirationDateForm {
  // Check if the expiration date form is already being shown.
  // After submitting a form and receiving an error result, this method might
  // get called to switch to the expiration date form. If the expiration date
  // form was already being displayed, no changes are needed.
  if (_expirationDateInputItem) {
    return;
  }

  // Load instructions for updating the expiration date.
  [self updateInstructions];

  [self addExpirationDateInputItem];

  // The footer is not displayed when updating the expiration date.
  [self removeFooterItem];

  // Change focus to expiration date field once the cells are loaded.
  _shouldUpdateFocus = YES;

  [self reloadAllSections];

  [self updateConfirmButtonState];

  // For Voice Over users, focus on the instructions to provide context about
  // what to do next.
  UIAccessibilityPostNotification(
      UIAccessibilityLayoutChangedNotification,
      [self.tableView headerViewForSection:[self.tableViewModel
                                               sectionForSectionIdentifier:
                                                   SectionIdentifierHeader]]);
}

// Displays a footer with a link to update the expiration date of the card.
// If the footer is already displayed, this method has no effects.
- (void)showUpdateExpirationDateLink {
  // Check if the link is already being displayed.
  // After submitting a form and receiving an error result, this method might
  // get called. If the link is already displayed, no changes are needed.
  if (_footerItem) {
    return;
  }

  _footerItem = [self createFooterItem];
  [self.tableViewModel setFooter:_footerItem
        forSectionWithIdentifier:SectionIdentifierInputs];

  // Restore focus to CVC input field after the section is reloaded.
  _shouldUpdateFocus = YES;

  // Reload inputs section to display footer.
  NSIndexSet* indexSet = [[NSIndexSet alloc]
      initWithIndex:[self.tableViewModel
                        sectionForSectionIdentifier:SectionIdentifierInputs]];
  [self.tableView reloadSections:indexSet
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Returns a newly created item for the header of the section.
- (CVCHeaderItem*)createHeaderItem {
  CVCHeaderItem* header = [[CVCHeaderItem alloc] initWithType:ItemTypeHeader];
  return header;
}

// Returns a new cancel button for the navigation bar.
- (UIBarButtonItem*)createCancelButton {
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onCancelTapped)];

  return cancelButton;
}

// Returns a new confirm button for the navigation bar.
- (UIBarButtonItem*)createConfirmButton {
  if (_bridge == nullptr) {
    return nil;
  }

  NSString* confirmButtonText =
      base::SysUTF16ToNSString(_bridge->GetController()->GetOkButtonLabel());
  UIBarButtonItem* confirmButton =
      [[UIBarButtonItem alloc] initWithTitle:confirmButtonText
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(onVerifyTapped)];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]
  }
                               forState:UIControlStateNormal];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
  }
                               forState:UIControlStateDisabled];

  return confirmButton;
}

// Returns the model for the CVC input cell.
- (TableViewTextEditItem*)createCVCInputItem {
  if (_bridge == nullptr) {
    return nil;
  }

  autofill::CardUnmaskPromptController* controller = _bridge->GetController();

  TableViewTextEditItem* CVCInputItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeCVCInput];
  CVCInputItem.delegate = self;
  CVCInputItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_AUTOFILL_CARD_UNMASK_PROMPT_CVC_FIELD_TITLE);
  CVCInputItem.keyboardType = UIKeyboardTypeNumberPad;
  CVCInputItem.hideIcon = YES;
  CVCInputItem.textFieldEnabled = YES;
  CVCInputItem.identifyingIcon = NativeImage(controller->GetCvcImageRid());

  return CVCInputItem;
}

// Returns a newly created item for the footer of the section.
- (TableViewLinkHeaderFooterItem*)createFooterItem {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_UPDATE_CARD_MESSAGE_LINK);
  // Using a dummy target for the link in the footer.
  // The link target is ignored and taps on it are handled by `didTapLinkURL`.
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kFooterDummyLinkTarget)] ];
  return footer;
}

// Removes the footer item from the table view model.
- (void)removeFooterItem {
  _footerItem = nil;

  [self.tableViewModel setFooter:nil
        forSectionWithIdentifier:SectionIdentifierInputs];
}

// Returns the model for the expiration date input cell.
- (ExpirationDateEditItem*)createExpirationDateInputItem {
  ExpirationDateEditItem* expirationDateInputItem =
      [[ExpirationDateEditItem alloc] initWithType:ItemTypeExpirationDateInput];
  expirationDateInputItem.delegate = self;
  expirationDateInputItem.fieldNameLabelText = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_EXPIRATION_DATE_FIELD_TITLE);
  return expirationDateInputItem;
}

// Adds the model for the expiration input item to the TableView model.
- (void)addExpirationDateInputItem {
  _expirationDateInputItem = [self createExpirationDateInputItem];
  [self.tableViewModel addItem:_expirationDateInputItem
       toSectionWithIdentifier:SectionIdentifierInputs];
}

// Updates the instructions in the header.
- (void)updateInstructions {
  _headerItem.instructionsText = [self instructions];
}

// Reloads all sections of the table view using automatic row animations.
// This method is used to update the views after the state of `self` changes.
// `reloadData` could work as well but preferring the animated aproach for
// smoother UX transitions.
- (void)reloadAllSections {
  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndexesInRange:NSMakeRange(
                                     0,
                                     [self.tableViewModel numberOfSections])];
  [self.tableView reloadSections:indexSet
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Returns YES if the CVC value entered matches the expected CVC format.
// The actual correctness of the CVC is verified by the server when the form is
// submitted.
- (BOOL)isCVCInputValid {
  if (_bridge == nullptr) {
    return NO;
  }

  return _bridge->GetController()->InputCvcIsValid(
      base::SysNSStringToUTF16(_CVCInputItem.textFieldValue));
}

// Returns YES if the expiration date entered matches the expected format or if
// the expiration date input is not being displayed. The actual correctness of
// the expiration date is verified by the server when the form is submitted.
- (BOOL)isExpirationInputValid {
  if (_bridge == nullptr) {
    return NO;
  }

  if (!_expirationDateInputItem) {
    return YES;
  }

  return _bridge->GetController()->InputExpirationIsValid(
      base::SysNSStringToUTF16(_expirationDateInputItem.month),
      base::SysNSStringToUTF16(_expirationDateInputItem.year));
}

// Enables the confirm button in the navigation bar iff the values for CVC and
// expiration date entered by the user are valid.
- (void)updateConfirmButtonState {
  _confirmButton.enabled =
      [self isCVCInputValid] && [self isExpirationInputValid];
}

// Updates `self` after the error alert was dismissed.
// When `closeOnDismiss` is YES, `self` is dismissed.
- (void)onErrorAlertDismissedAndShouldCloseOnDismiss:(BOOL)closeOnDismiss {
  if (_bridge == nullptr) {
    return;
  }

  if (closeOnDismiss) {
    _bridge->PerformClose();
    return;
  }
  // Interactions were disabled in the loading state.
  // Enabling them after the alert is dismissed.
  self.tableView.userInteractionEnabled = YES;
  // Check if we need to switch to the update expiration date form.
  if (_bridge->GetController()->ShouldRequestExpirationDate()) {
    [self showUpdateExpirationDateForm];
  } else {
    // Display the expiration date link after an error verifying the CVC.
    [self showUpdateExpirationDateLink];
  }
}

// Helper method that fetches the instructions from the Controller.
- (NSString*)instructions {
  if (_bridge == nullptr) {
    return nil;
  }
  autofill::CardUnmaskPromptController* controller = _bridge->GetController();
  return base::SysUTF16ToNSString(controller->GetInstructionsMessage());
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  [self updateConfirmButtonState];
}

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  // Adding space on top of the inputs section to match the mocks' spacing.
  NSInteger inputsSection =
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierInputs];
  if (section == inputsSection) {
    return kInputsSectionTopSpacing;
  }
  return UITableViewAutomaticDimension;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // The header section doesn't need a footer, settings its height to zero to
  // avoid extra spacing between sections.
  NSInteger headerSection =
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierHeader];
  if (section == headerSection) {
    return 0;
  }
  // Let Autolayout calculate calculate the footer's height if any.
  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  // Default spacing when no footer.
  return kEmptyFooterHeight;
}

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only update focus for cells with input fields and when update focus is
  // needed.
  // Don't update focus when Voice Over is running. Instead, the instructions
  // will be read, providing more context for users with Voice Over enabled.
  if (UIAccessibilityIsVoiceOverRunning() || !_shouldUpdateFocus ||
      ![cell isKindOfClass:TableViewTextEditCell.class]) {
    return;
  }
  // When we're about to display the CVC form or expiration date form for
  // the first time focus the textField on the right cell.
  TableViewTextEditCell* rowCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);

  ItemType rowItemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  // If the expiration date cell is displayed then we're transitioning to the
  // expiration date form. Only focus the CVC cell when we're not transitioning
  // to the expiration date form.
  if (rowItemType == ItemTypeExpirationDateInput ||
      (rowItemType == ItemTypeCVCInput && !_expirationDateInputItem)) {
    [rowCell.textField becomeFirstResponder];
    _shouldUpdateFocus = NO;
  }
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  // Set `self` as delegate for the inputs section footer to handle taps on the
  // Update Card link.
  if (sectionIdentifier == SectionIdentifierInputs) {
    TableViewLinkHeaderFooterView* footerView =
        base::mac::ObjCCast<TableViewLinkHeaderFooterView>(view);
    footerView.delegate = self;
  }

  return view;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType rowItemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (rowItemType == ItemTypeCVCInput) {
    TableViewTextEditCell* rowCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
    rowCell.textField.delegate = self;
    // Hide the icon from Voice Over.
    rowCell.identifyingIconButton.isAccessibilityElement = NO;
  }

  return cell;
}

#pragma mark - TableViewLinkHeaderFooterDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (_bridge == nullptr) {
    return;
  }
  // Notify Controller about the Expiration Date Form being shown so it updates
  // its state accordingly.
  _bridge->GetController()->NewCardLinkClicked();
  // Handle taps on the Update Card link.
  [self showUpdateExpirationDateForm];
}

#pragma mark - ExpirationDateEditItemDelegate

- (void)expirationDateEditItemDidChange:(ExpirationDateEditItem*)item {
  // Handle expiration date selections.
  [self updateConfirmButtonState];
}

#pragma mark - UITextFieldDelegate

// CVC input is limited to 4 characters since CVCs are never longer than that.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  return (textField.text.length + string.length - range.length) <= 4;
}

@end
