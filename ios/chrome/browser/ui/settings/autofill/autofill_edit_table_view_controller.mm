// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_chromium_text_data.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_edit_table_view_controller+protected.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillEditTableViewController () <FormInputAccessoryViewDelegate> {
  TableViewTextEditCell* _currentEditingCell;
}

// The accessory view when editing any of text fields.
@property(nonatomic, strong) FormInputAccessoryView* formInputAccessoryView;

@end

@implementation AutofillEditTableViewController

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (!self) {
    return nil;
  }

  _formInputAccessoryView = [[FormInputAccessoryView alloc] init];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.formInputAccessoryView setUpWithLeadingView:nil
                                 navigationDelegate:self];
  [self setShouldHideDoneButton:YES];
  [self updateUIForEditState];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidShow)
             name:UIKeyboardDidShowNotification
           object:nil];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardDidShowNotification
              object:nil];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  self.formInputAccessoryView.hidden = IsCompactHeight(self);
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return YES;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.tableView.editing;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  TableViewTextEditCell* cell = [self autofillEditCellForTextField:textField];
  _currentEditingCell = cell;
  if (!IsCompactHeight(self)) {
    self.formInputAccessoryView.hidden = NO;
  }
  [textField setInputAccessoryView:self.formInputAccessoryView];
  [self updateAccessoryViewButtonState];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  TableViewTextEditCell* cell = [self autofillEditCellForTextField:textField];
  DCHECK(_currentEditingCell == cell);
  [textField setInputAccessoryView:nil];
  self.formInputAccessoryView.hidden = YES;
  _currentEditingCell = nil;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK([_currentEditingCell textField] == textField);
  [self moveToAnotherTextFieldWithOffset:1];
  return NO;
}

#pragma mark - FormInputAccessoryViewDelegate

- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender {
  [self moveToAnotherTextFieldWithOffset:1];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  [self moveToAnotherTextFieldWithOffset:-1];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [[_currentEditingCell textField] resignFirstResponder];
}

- (FormInputAccessoryViewTextData*)textDataforFormInputAccessoryView:
    (FormInputAccessoryView*)sender {
  return ChromiumAccessoryViewTextData();
}

#pragma mark - Helper methods

// Returns the cell containing `textField`.
- (TableViewTextEditCell*)autofillEditCellForTextField:(UITextField*)textField {
  TableViewTextEditCell* settingsCell = nil;
  for (UIView* view = textField; view; view = [view superview]) {
    TableViewTextEditCell* cell =
        base::mac::ObjCCast<TableViewTextEditCell>(view);
    if (cell) {
      settingsCell = cell;
      break;
    }
  }

  // There should be a cell associated with this text field.
  DCHECK(settingsCell);
  return settingsCell;
}

- (NSIndexPath*)indexForNextCellPathWithOffset:(NSInteger)offset
                                      fromPath:(NSIndexPath*)cellPath {
  if (!cellPath || !offset) {
    return nil;
  }

  NSInteger cellSection = [cellPath section];
  NSInteger nextCellRow = [cellPath row] + offset;

  while (cellSection >= 0 && cellSection < [self.tableView numberOfSections]) {
    while (nextCellRow >= 0 &&
           nextCellRow < [self.tableView numberOfRowsInSection:cellSection]) {
      NSIndexPath* cellIndexPath = [NSIndexPath indexPathForRow:nextCellRow
                                                      inSection:cellSection];
      if ([self isItemAtIndexPathTextEditCell:cellIndexPath]) {
        return cellIndexPath;
      }
      nextCellRow += offset;
    }

    cellSection += offset;
    if (offset > 0) {
      nextCellRow = 0;
    } else {
      if (cellSection >= 0) {
        nextCellRow = [self.tableView numberOfRowsInSection:cellSection] - 1;
      }
    }
  }

  return nil;
}

- (NSIndexPath*)indexPathForCurrentTextField {
  DCHECK(_currentEditingCell);
  return [self.tableView indexPathForCell:_currentEditingCell];
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  return YES;
}

- (void)moveToAnotherTextFieldWithOffset:(NSInteger)offset {
  NSIndexPath* currentCellPath = [self indexPathForCurrentTextField];
  DCHECK(currentCellPath);

  NSIndexPath* nextCellPath =
      [self indexForNextCellPathWithOffset:offset fromPath:currentCellPath];

  if (nextCellPath) {
    TableViewTextEditCell* nextCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(
            [self.tableView cellForRowAtIndexPath:nextCellPath]);
    [nextCell.textField becomeFirstResponder];
  } else {
    [[_currentEditingCell textField] resignFirstResponder];
  }
}

- (void)updateAccessoryViewButtonState {
  NSIndexPath* currentPath = [self indexPathForCurrentTextField];
  NSIndexPath* nextPath = [self indexForNextCellPathWithOffset:1
                                                      fromPath:currentPath];
  NSIndexPath* previousPath = [self indexForNextCellPathWithOffset:-1
                                                          fromPath:currentPath];

  BOOL isValidPreviousPath =
      previousPath && [[self.tableView cellForRowAtIndexPath:previousPath]
                          isKindOfClass:TableViewTextEditCell.class];
  self.formInputAccessoryView.previousButton.enabled = isValidPreviousPath;

  BOOL isValidNextPath =
      nextPath && [[self.tableView cellForRowAtIndexPath:nextPath]
                      isKindOfClass:TableViewTextEditCell.class];
  self.formInputAccessoryView.nextButton.enabled = isValidNextPath;
}

#pragma mark - Keyboard handling

- (void)keyboardDidShow {
  [self.tableView scrollRectToVisible:[_currentEditingCell frame] animated:YES];
}

@end
