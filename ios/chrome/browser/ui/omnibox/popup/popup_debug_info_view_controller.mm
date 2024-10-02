// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/popup_debug_info_view_controller.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Debug text view used to display text that can be selected.
UITextView* DebugTextView() {
  UITextView* textView = [[UITextView alloc] init];
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.editable = NO;
  textView.scrollEnabled = NO;
  textView.font = [UIFont systemFontOfSize:15];
  return textView;
}

/// Text field used to input variation id.
UITextField* ForceVariationTextField() {
  UITextField* textField = [[UITextField alloc] init];
  textField.translatesAutoresizingMaskIntoConstraints = NO;
  textField.borderStyle = UITextBorderStyleBezel;
  textField.backgroundColor = UIColor.lightGrayColor;
  textField.keyboardType = UIKeyboardTypeNumberPad;
  textField.placeholder = @"Force variation ID";
  return textField;
}

/// Stack view containing variation id information.
UIStackView* VariationStackView() {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.spacing = 10;
  return stackView;
}

/// Button to open app settings.
UIButton* SettingsButton() {
  UIAction* openSettings = [UIAction actionWithHandler:^(UIAction* action) {
    NSURL* url =
        [[NSURL alloc] initWithString:UIApplicationOpenSettingsURLString];
    [[UIApplication sharedApplication] openURL:url
                                       options:@{}
                             completionHandler:nil];
  }];
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration grayButtonConfiguration];
  configuration.title = @"Open iOS Settings";
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightLight];
  configuration.image = DefaultSymbolWithConfiguration(@"gear.circle", config);
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:openSettings];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

/// Label showing instrunction to force variation Id.
UILabel* VariationInstructionLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.text = @"Copy these in iOS Settings > Experimental Settings > EXTRA "
               @"FLAGS (ONE PER LINE)";
  return label;
}

}  // namespace

@interface PopupDebugInfoViewController () <UITextFieldDelegate>

@property(nonatomic, strong) UITextView* activeVariationIDTextView;
@property(nonatomic, strong) UITextField* variationIDTextField;
@property(nonatomic, strong) UITextView* enableVariationIDTextView;
@property(nonatomic, strong) UITextView* disableVariationIDsTextView;

@property(nonatomic, strong) UILabel* variationInstructionLabel;
@property(nonatomic, strong) UIButton* settingsButton;

@property(nonatomic, strong) NSArray<NSNumber*>* activeVariationIDs;
@property(nonatomic, strong) UIStackView* variationStackView;

@end

@implementation PopupDebugInfoViewController

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _activeVariationIDs = @[];
    _variationStackView = VariationStackView();
    _activeVariationIDTextView = DebugTextView();
    _variationIDTextField = ForceVariationTextField();
    _variationIDTextField.delegate = self;
    _settingsButton = SettingsButton();
    _variationInstructionLabel = VariationInstructionLabel();
    _enableVariationIDTextView = DebugTextView();
    _disableVariationIDsTextView = DebugTextView();

    [_variationIDTextField addTarget:self
                              action:@selector(textFieldDidChange:)
                    forControlEvents:UIControlEventEditingChanged];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.backgroundColor = UIColor.systemBackgroundColor;
  [self.view addSubview:scrollView];
  AddSameConstraints(self.view, scrollView);

  UIStackView* stackView = self.variationStackView;
  [scrollView addSubview:stackView];
  AddSameConstraints(stackView, scrollView);
  [NSLayoutConstraint activateConstraints:@[
    [scrollView.widthAnchor constraintEqualToAnchor:stackView.widthAnchor],
    [scrollView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
  ]];

  [stackView addArrangedSubview:self.activeVariationIDTextView];
  [stackView addArrangedSubview:self.variationIDTextField];
  [stackView addArrangedSubview:self.settingsButton];
  [stackView addArrangedSubview:self.variationInstructionLabel];
  [stackView addArrangedSubview:self.enableVariationIDTextView];
  [stackView addArrangedSubview:self.disableVariationIDsTextView];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonPressed)];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  if (textField == self.variationIDTextField) {
    if (!string.length) {
      return YES;
    }
    NSCharacterSet* nonDecimalDigit =
        [NSCharacterSet decimalDigitCharacterSet].invertedSet;
    if ([string rangeOfCharacterFromSet:nonDecimalDigit].location ==
        NSNotFound) {
      return YES;
    }

    // Allow pasting variation ids with a leading t.
    if ([string characterAtIndex:0] == 't') {
      NSString* substring = [string substringFromIndex:1];
      if ([substring intValue]) {
        textField.text = substring;
      }
    }
    return NO;
  }
  return YES;
}

- (void)textFieldDidChange:(id)sender {
  [self updateForceVariationTextViews];
}

#pragma mark - PopupDebugInfoConsumer

- (void)setVariationIDString:(NSString*)string {
  NSCharacterSet* whitespaceSet =
      [NSCharacterSet whitespaceAndNewlineCharacterSet];
  NSString* trimmedString =
      [string stringByTrimmingCharactersInSet:whitespaceSet];

  self.activeVariationIDTextView.text =
      [NSString stringWithFormat:@"Active variation IDs: %@", trimmedString];

  NSArray<NSString*>* stringIds =
      [trimmedString componentsSeparatedByString:@" "];
  self.activeVariationIDs = [[stringIds valueForKey:@"intValue"]
      filteredArrayUsingPredicate:[NSPredicate
                                      predicateWithFormat:@"SELF != 0"]];
  [self updateForceVariationTextViews];
}

#pragma mark - AutocompleteControllerObserver

- (void)autocompleteController:(AutocompleteController*)controller
             didStartWithInput:(const AutocompleteInput&)input {
}

- (void)autocompleteController:(AutocompleteController*)controller
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged {
}

#pragma mark - RemoteSuggestionsServiceObserver

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
                 startingRequest:(const network::ResourceRequest*)request
                uniqueIdentifier:
                    (const base::UnguessableToken&)requestIdentifier {
}

- (void)remoteSuggestionsService:(RemoteSuggestionsService*)service
    completedRequestWithIdentifier:
        (const base::UnguessableToken&)requestIdentifier
                  receivedResponse:(NSString*)response {
}

#pragma mark - private

- (void)doneButtonPressed {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)updateForceVariationTextViews {
  NSInteger forcedId = [self.variationIDTextField.text intValue];
  NSString* enabledVariationString = @"";
  NSString* disabledVariationString = @"";

  if (forcedId > 0) {
    enabledVariationString = [NSString
        stringWithFormat:@"%s=t%ld", variations::switches::kForceVariationIds,
                         forcedId];

    NSArray* disabledIds = [self.activeVariationIDs
        filteredArrayUsingPredicate:
            [NSPredicate
                predicateWithFormat:@"SELF != %@",
                                    [NSNumber numberWithInteger:forcedId]]];
    if (disabledIds.count) {
      disabledVariationString = [NSString
          stringWithFormat:@"%s=t%@",
                           variations::switches::kForceDisableVariationIds,
                           [disabledIds componentsJoinedByString:@",t"]];
    }
  }

  self.enableVariationIDTextView.text = enabledVariationString;
  self.disableVariationIDsTextView.text = disabledVariationString;
  self.enableVariationIDTextView.hidden =
      !self.enableVariationIDTextView.text.length;
  self.disableVariationIDsTextView.hidden =
      !self.disableVariationIDsTextView.text.length;
  self.variationInstructionLabel.hidden =
      self.enableVariationIDTextView.hidden &&
      self.disableVariationIDsTextView.hidden;
  self.settingsButton.hidden = self.variationInstructionLabel.hidden;
}

@end
