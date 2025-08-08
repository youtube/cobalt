// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"

#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

NSString* const kVoiceSearchInputAccessoryViewID =
    @"kVoiceSearchInputAccessoryViewID";

NSString* const kPasteSearchInputAccessoryViewID =
    @"kPasteSearchInputAccessoryViewID";

// Parameters for the appearance of the buttons.
const CGFloat kOmniboxAssistiveKeyboardSymbolPointSize = 23.0;
const CGFloat kSymbolButtonSize = 36.0;
const CGFloat kPasteButtonSize = 36.0;
const CGFloat kButtonShadowOpacity = 0.35;
const CGFloat kButtonShadowRadius = 1.0;
const CGFloat kButtonShadowVerticalOffset = 1.0;

namespace {

void SetUpButtonWithIcon(UIButton* button, NSString* iconName) {
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [UIImage imageNamed:iconName];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
}

void SetUpButtonWithSymbol(UIButton* button,
                           NSString* symbolName,
                           BOOL isCustomSymbol) {
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kOmniboxAssistiveKeyboardSymbolPointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];

  UIImage* icon =
      isCustomSymbol
          ? CustomSymbolWithConfiguration(symbolName, configuration)
          : DefaultSymbolWithConfiguration(symbolName, configuration);
  if (UITraitCollection.currentTraitCollection.userInterfaceStyle ==
      UIUserInterfaceStyleDark) {
    icon = MakeSymbolMonochrome(icon);
    button.tintColor = [UIColor whiteColor];
  } else {
    icon = MakeSymbolMulticolor(icon);
  }

  button.backgroundColor = [UIColor colorNamed:kOmniboxKeyboardButtonColor];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.cornerRadius = kSymbolButtonSize / 2;

  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;

  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor constraintEqualToConstant:kSymbolButtonSize],
    [button.heightAnchor constraintEqualToConstant:kSymbolButtonSize]
  ]];
}

}  // namespace

void UpdateLensButtonAppearance(UIButton* button) {
  SetUpButtonWithSymbol(button, kCameraLensSymbol, YES);
}

NSArray<UIControl*>* OmniboxAssistiveKeyboardLeadingControls(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    id<UIPasteConfigurationSupporting> pasteTarget,
    bool useLens) {
  NSMutableArray<UIControl*>* controls = [NSMutableArray<UIControl*> array];

  UIButton* voiceSearchButton =
      [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero];
  SetUpButtonWithIcon(voiceSearchButton, @"keyboard_accessory_voice_search");
  voiceSearchButton.enabled = ios::provider::IsVoiceSearchEnabled();
  NSString* accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH);
  voiceSearchButton.accessibilityLabel = accessibilityLabel;
  voiceSearchButton.accessibilityIdentifier = kVoiceSearchInputAccessoryViewID;
  [voiceSearchButton addTarget:delegate
                        action:@selector(keyboardAccessoryVoiceSearchTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  [controls addObject:voiceSearchButton];

  UIButton* cameraButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
  if (useLens) {
    // Set up the camera button for Lens.
    delegate.lensButton = cameraButton;
    [delegate.layoutGuideCenter referenceView:cameraButton
                                    underName:kLensKeyboardButtonGuide];
    UpdateLensButtonAppearance(cameraButton);
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryLensTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(cameraButton,
                                    IDS_IOS_KEYBOARD_ACCESSORY_VIEW_LENS,
                                    @"Search With Lens");
  } else {
    // Set up the camera button for the QR scanner.
    SetUpButtonWithIcon(cameraButton, @"keyboard_accessory_qr_scanner");
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryCameraSearchTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(
        cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
        @"QR code Search");
  }
  [controls addObject:cameraButton];

  if (base::FeatureList::IsEnabled(kOmniboxKeyboardPasteButton)) {
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
    if (@available(iOS 16, *)) {
      [controls addObject:OmniboxAssistiveKeyboardPasteControl(pasteTarget)];
    }
#endif  // defined(__IPHONE_16_0)
  }
  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    UIButton* debuggerButton =
        [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero];
    SetUpButtonWithSymbol(debuggerButton, kSettingsSymbol, NO);
    [debuggerButton addTarget:delegate
                       action:@selector(keyboardAccessoryDebuggerTapped)
             forControlEvents:UIControlEventTouchUpInside];
    [controls addObject:debuggerButton];
  }

  return controls;
}

#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
UIPasteControl* OmniboxAssistiveKeyboardPasteControl(
    id<UIPasteConfigurationSupporting> pasteTarget) API_AVAILABLE(ios(16)) {
  UIPasteControlConfiguration* pasteControlConfiguration =
      [[UIPasteControlConfiguration alloc] init];
  [pasteControlConfiguration setDisplayMode:UIPasteControlDisplayModeIconOnly];
  [pasteControlConfiguration
      setCornerStyle:UIButtonConfigurationCornerStyleCapsule];
  [pasteControlConfiguration setBaseBackgroundColor:UIColor.systemBlueColor];
  [pasteControlConfiguration setBaseForegroundColor:UIColor.whiteColor];
  UIPasteControl* pasteControl =
      [[UIPasteControl alloc] initWithConfiguration:pasteControlConfiguration];
  pasteControl.target = pasteTarget;
  pasteControl.accessibilityIdentifier = kPasteSearchInputAccessoryViewID;
  // Set as accessiblity hint since UIPasteControl already set the
  // accessiblityLabel.
  pasteControl.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_PASTE_SEARCH);
  // Set content size category to extra small to reduce the size of the paste
  // icon.
  [pasteControl setMaximumContentSizeCategory:UIContentSizeCategoryExtraSmall];
  [pasteControl setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [pasteControl.widthAnchor constraintEqualToConstant:kPasteButtonSize],
    [pasteControl.heightAnchor constraintEqualToConstant:kPasteButtonSize]
  ]];
  // Hide `pasteControl` when there is no content in the pasteboard or when the
  // content cannot be pasted into the omnibox.
  __weak UIPasteControl* weakControl = pasteControl;
  void (^setPasteButtonHiddenState)(NSNotification*) =
      ^(NSNotification* notification) {
        BOOL pasteButtonShouldBeVisible =
            [UIPasteboard.generalPasteboard hasStrings] ||
            [UIPasteboard.generalPasteboard hasURLs] ||
            [UIPasteboard.generalPasteboard hasImages];
        [weakControl setHidden:!pasteButtonShouldBeVisible];
      };
  setPasteButtonHiddenState(nil);
  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIPasteboardChangedNotification
                  object:nil
                   queue:nil
              usingBlock:setPasteButtonHiddenState];
  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:setPasteButtonHiddenState];
  return pasteControl;
}
#endif  // defined(__IPHONE_16_0)
