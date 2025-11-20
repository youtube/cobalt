// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "starboard/tvos/shared/application_window.h"

#import <GLKit/GLKit.h>

#include <unordered_set>

#include "starboard/event.h"
#import "starboard/tvos/shared/application_darwin.h"
#import "starboard/tvos/shared/application_view.h"
#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/keyboard_input_device.h"
#import "starboard/tvos/shared/media/application_player.h"
#import "starboard/tvos/shared/media/egl_surface.h"
#import "starboard/tvos/shared/search_results_view_controller.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

using starboard::ApplicationDarwin;

/**
 *  @brief Debounce constant for updateSearchResultsForSearchController voice
 * query input.
 */
static const NSTimeInterval kSearchResultDebounceTime = 0.5;

/**
 *  @brief A view's delegate is made aware of view lifecycle changes.
 */
@protocol SBDSearchViewDisplayDelegate

/**
 *  @brief Called when the search viewDidAppear.
 */
- (void)searchDidAppear;

/**
 *  @brief Called when the search viewDidDisappear.
 */
- (void)searchDidDisappear;

@end

/**
 *  @brief The @c UISearchContainerViewController that should be used for
 *      displaying a @c UISearchController.
 */
@interface SBDSearchContainerViewController : UISearchContainerViewController

/**
 *  @brief The delegate to notify of display changes to the search view.
 */
@property(weak, nonatomic) id<SBDSearchViewDisplayDelegate> delegate;

@end

/**
 *  @brief The @c UIViewController for the EGL surface and player views.
 */
@interface SBDApplicationWindowViewController
    : UIViewController <UISearchResultsUpdating,
                        UISearchControllerDelegate,
                        SBDSearchResultsFocusDelegate,
                        SBDSearchViewDisplayDelegate>

/**
 *  @brief The Starboard application's viewport.
 */
@property(nonatomic, readonly) SBDApplicationView* applicationView;

/**
 *  @brief When YES, indicates the on-screen keyboard is showing.
 */
@property(nonatomic, readonly) BOOL keyboardShowing;

/**
 *  @brief Indicates whether the on-screen keyboard is focused.
 */
@property(nonatomic, readonly) BOOL keyboardFocused;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

/**
 *  @brief Designated initializer.
 */
- (instancetype)init NS_DESIGNATED_INITIALIZER;

/**
 *  @brief Returns the current frame or a zero rect if the on-screen keyboard
 *      isn't showing.
 */
- (SbWindowRect)getOnScreenKeyboardFrame;

/**
 *  @brief Shows the on-screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard is already
 *      shown (in which case, this method is a no-op). It is expected that the
 *      @c interfaceContainer's frame has been changed before this method
 *      returns.
 */
- (void)showOnScreenKeyboardWithText:(NSString*)text ticket:(NSInteger)ticket;

/**
 *  @brief Hides any visible on-screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard isn't being shown. It
 *      is expected that the @c interfaceContainer's frame has been changed
 *      before this method returns.
 */
- (void)hideOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief When @c keepFocus is @c YES, the on-screen keyboard should not allow
 *      focus to be lost.
 *  @param keepFocus Whether the on-screen keyboard should allow focus to be
 *      lost.
 */
- (void)onScreenKeyboardKeepFocus:(BOOL)keepFocus;

/**
 *  @brief Focuses the on-screen keyboard.
 */
- (void)focusOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief Removes focus from the on-screen keyboard.
 */
- (void)blurOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief Set on-screen keyboard's background color in RGB colorspace.
 *  @param red, green, blue, alpha are specified as a value from 0.0 to 1.0.
 */
- (void)setOnScreenKeyboardBackgroundColorWithRed:(CGFloat)red
                                            green:(CGFloat)green
                                             blue:(CGFloat)blue
                                            alpha:(CGFloat)alpha;

/**
 *  @brief When @c lightTheme is @c YES, the on-screen keyboard should switch to
 *      light theme.
 *  @param lightTheme Whether the on-screen keyboard should  enable light theme.
 */
- (void)setOnScreenKeyboardLightTheme:(BOOL)lightTheme;

@end

@implementation SBDApplicationWindow {
  /**
   *  @brief The @c UIViewController for EGL and Player views.
   */
  SBDApplicationWindowViewController* _viewController;

  /**
   *  @brief The preferred focus item. This is only transiently set for a
   *      focus update, then unset.
   */
  id<UIFocusEnvironment> _focusOverride;

  /**
   *  @brief Cache the window properties so it can be accessed from any thread.
   */
  SbWindowSize _windowSize;

  /**
   *  @brief Track the set of keydown UIPresses that need a keyup sent. This is
   *      used to ensure that a keyup is sent every time a keydown was sent.
   *      The set is only used to track navigation UIPresses at the moment.
   */
  std::unordered_set<NSInteger> _processedKeydownPressTypes;
}

- (instancetype)initWithName:(nullable NSString*)name {
  CGSize size = [UIScreen mainScreen].bounds.size;
  self = [super initWithFrame:CGRectMake(0, 0, size.width, size.height)];
  if (self) {
    _keyboardInputDevice = [[SBDKeyboardInputDevice alloc] init];
    _viewController = [[SBDApplicationWindowViewController alloc] init];

    CGFloat scale = [UIScreen mainScreen].scale;
    _windowSize.width = static_cast<int>(size.width * scale);
    _windowSize.height = static_cast<int>(size.height * scale);
    _windowSize.video_pixel_ratio = 1.0f;

    _processUIPresses = YES;

    self.rootViewController = _viewController;
    self.accessibilityIdentifier = name;
    self.backgroundColor = [SBDApplicationWindow defaultWindowColor];
  }
  return self;
}

- (NSArray<id<UIFocusEnvironment>>*)preferredFocusEnvironments {
  if (_focusOverride) {
    return @[ _focusOverride ];
  }
  return @[ self ];
}

#pragma mark - SBDApplicationWindow

+ (UIColor*)defaultWindowColor {
  NSString* colorFromPList = [[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"YTApplicationBackgroundColor"];
  NSArray<NSString*>* colorComponents =
      [colorFromPList componentsSeparatedByString:@", "];
  if (colorComponents.count == 4) {
    return [UIColor colorWithRed:[colorComponents[0] floatValue] / 255.0
                           green:[colorComponents[1] floatValue] / 255.0
                            blue:[colorComponents[2] floatValue] / 255.0
                           alpha:[colorComponents[3] floatValue]];
  } else {
    return [UIColor colorWithRed:30 / 255.0
                           green:30 / 255.0
                            blue:30 / 255.0
                           alpha:1];
  }
}

- (NSString*)name {
  return self.accessibilityIdentifier;
}

- (SbWindowSize)size {
  // This implementation must be safe to call outside of the main thread.
  return _windowSize;
}

- (void)updateWindowSize {
  CGSize size = [UIScreen mainScreen].bounds.size;
  CGFloat scale = [UIScreen mainScreen].scale;
  _windowSize.width = static_cast<int>(size.width * scale);
  _windowSize.height = static_cast<int>(size.height * scale);
  _windowSize.video_pixel_ratio = 1.0f;
  return;
}

- (BOOL)windowed {
  return NO;
}

/**
 *  @brief A mapping from UIKit key command input to the appropriate SbKey.
 */
static const NSDictionary<NSString*, NSNumber*>* keyCommandToSbKey = @{
  @"\b" : @(kSbKeyBackspace),
  @"\t" : @(kSbKeyTab),

  // Do not map UIKeyInput @"\r" to SbKey, as it duplicatets with UIPressType
  // UIPressTypeSelect. A single return key press on a BT keyboard will trigger
  // both a @"\r" UIKeyInput press and a UIPressTypeSelect UIPressType press.
  // And we should send only UIPressTypeSelect press to _keyboardInputDevice.
  // @"\r" : @(kSbKeyReturn),

  // Do not map any navigation UIKeyInput to SbKey, as navigation is handled by
  // the system focus engine of the hybrid nav, there's no need sending them to
  // _keyboardInputDevice. UIKeyInputUpArrow : @(kSbKeyUp), UIKeyInputDownArrow
  // : @(kSbKeyDown), UIKeyInputLeftArrow : @(kSbKeyLeft), UIKeyInputRightArrow
  // : @(kSbKeyRight),

  @"0" : @(kSbKey0),
  @"1" : @(kSbKey1),
  @"2" : @(kSbKey2),
  @"3" : @(kSbKey3),
  @"4" : @(kSbKey4),
  @"5" : @(kSbKey5),
  @"6" : @(kSbKey6),
  @"7" : @(kSbKey7),
  @"8" : @(kSbKey8),
  @"9" : @(kSbKey9),

  @"a" : @(kSbKeyA),
  @"b" : @(kSbKeyB),
  @"c" : @(kSbKeyC),
  @"d" : @(kSbKeyD),
  @"e" : @(kSbKeyE),
  @"f" : @(kSbKeyF),
  @"g" : @(kSbKeyG),
  @"h" : @(kSbKeyH),
  @"i" : @(kSbKeyI),
  @"j" : @(kSbKeyJ),
  @"k" : @(kSbKeyK),
  @"l" : @(kSbKeyL),
  @"m" : @(kSbKeyM),
  @"n" : @(kSbKeyN),
  @"o" : @(kSbKeyO),
  @"p" : @(kSbKeyP),
  @"q" : @(kSbKeyQ),
  @"r" : @(kSbKeyR),
  @"s" : @(kSbKeyS),
  @"t" : @(kSbKeyT),
  @"u" : @(kSbKeyU),
  @"v" : @(kSbKeyV),
  @"w" : @(kSbKeyW),
  @"x" : @(kSbKeyX),
  @"y" : @(kSbKeyY),
  @"z" : @(kSbKeyZ),

  @"`" : @(kSbKeyOem3),
  @"-" : @(kSbKeyOemMinus),
  @"=" : @(kSbKeyOemPlus),
  @"[" : @(kSbKeyOem4),
  @"]" : @(kSbKeyOem6),
  @"\\" : @(kSbKeyOem5),
  @";" : @(kSbKeyOem1),
  @"'" : @(kSbKeyOem7),
  @"," : @(kSbKeyOemComma),
  @"." : @(kSbKeyOemPeriod),
  @"/" : @(kSbKeyOem2),
  @" " : @(kSbKeySpace),
};

- (SbKey)mapKeyToSbKeyWithModifiers:(UIPress*)press
                      withModifiers:(SbKeyModifiers*)modifiers {
  *modifiers = kSbKeyModifiersNone;
  switch (press.type) {
    // Do not handle any navigation UIPressTypes and UIPressTypeSelect in
    // (void)pressesBegan:(NSSet<UIPress*>*)presses
    // withEvent:(UIPressesEvent*)event, but process them in
    // (void)sendEvent:(UIEvent*)event. case UIPressTypeUpArrow:
    //   return kSbKeyUp;
    // case UIPressTypeDownArrow:
    //   return kSbKeyDown;
    // case UIPressTypeLeftArrow:
    //   return kSbKeyLeft;
    // case UIPressTypeRightArrow:
    //   return kSbKeyRight;
    // case UIPressTypeSelect:
    //   return kSbKeyReturn;

    // Handle the only two action UIPressTypes.
    case UIPressTypePlayPause:
      return kSbKeyMediaPlayPause;
    case UIPressTypeMenu:
      return kSbKeyEscape;
    default:
      break;
  }

  auto key = press.key;
  if (key.modifierFlags & UIKeyModifierAlternate) {
    *modifiers = static_cast<SbKeyModifiers>(*modifiers | kSbKeyModifiersAlt);
  }
  if (key.modifierFlags & UIKeyModifierControl) {
    *modifiers = static_cast<SbKeyModifiers>(*modifiers | kSbKeyModifiersCtrl);
  }
  if (key.modifierFlags & UIKeyModifierShift) {
    *modifiers = static_cast<SbKeyModifiers>(*modifiers | kSbKeyModifiersShift);
  }
  return static_cast<SbKey>(
      [keyCommandToSbKey objectForKey:key.charactersIgnoringModifiers]
          .intValue);
}

- (void)pressesBegan:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    // When on-screen keyboard is focused:
    if ([self keyboardFocused]) {
      if (press.type == UIPressTypeSelect) {
        // Do not handle UIPressTypeSelect from remote on on-screen keyboard, as
        // text selection is managed by
        // (void)updateSearchResultsForSearchController:(UISearchController
        // *)searchController. Do not forward the presses to superclass or send
        // them to _keyboardInputDevice.
        return;
      } else if (press.type != UIPressTypeMenu) {
        // For UIPressTypeMenu presses, forward the presses to superclass and
        // send them to _keyboardInputDevice. For any other presses, forward the
        // presses to superclass, but do not send them to _keyboardInputDevice.
        continue;
      }
    }

    // Do not handle any navigation UIPressTypes in
    // (void)pressesBegan:(NSSet<UIPress*>*)presses
    // withEvent:(UIPressesEvent*)event, but process them in
    // (void)sendEvent:(UIEvent*)event.
    if (press.type == UIPressTypeUpArrow ||
        press.type == UIPressTypeDownArrow ||
        press.type == UIPressTypeLeftArrow ||
        press.type == UIPressTypeRightArrow) {
      return;
    }

    SbKeyModifiers modifiers = kSbKeyModifiersNone;
    SbKey sbkey = [self mapKeyToSbKeyWithModifiers:press
                                     withModifiers:&modifiers];

    if (sbkey == kSbKeyUnknown) {
      // Skip any unknown presses, this includes redundant UIKeyInput @"\r"
      // presses and UIKeyInput navigation presses.
      continue;
    }

    // TODO: Is _processUIPresses still needed?
    // Only initiate a keydown if UIPresses should be processed.
    if (_processUIPresses) {
      [_keyboardInputDevice keyPressed:sbkey modifiers:modifiers];
    }
  }
  [super pressesBegan:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    if ([self keyboardFocused]) {
      if (press.type == UIPressTypeSelect) {
        return;
      } else if (press.type != UIPressTypeMenu) {
        continue;
      }
    }

    if (press.type == UIPressTypeUpArrow ||
        press.type == UIPressTypeDownArrow ||
        press.type == UIPressTypeLeftArrow ||
        press.type == UIPressTypeRightArrow) {
      return;
    }

    SbKeyModifiers modifiers = kSbKeyModifiersNone;
    SbKey sbkey = [self mapKeyToSbKeyWithModifiers:press
                                     withModifiers:&modifiers];

    if (sbkey == kSbKeyUnknown) {
      continue;
    }

    [_keyboardInputDevice keyUnpressed:sbkey modifiers:modifiers];
  }
  [super pressesEnded:presses withEvent:event];
}

// TODO: deprecate (void)sendEvent:(UIEvent*)event.
// We should not override (void)sendEvent:(UIEvent*)event to intercept events,
// but instead, we should move the logic to the publicly available API for
// handling press events using pressesBegan/pressesEnded. Currently we still
// need this function to send the four navigation UIPressTypes to Kabuki,
// because Kabuki correctly handles the events from this function under
// different cases. e.g. scrubbing videso when receiving a kSbKeyRight event,
// but not moving focus on sign in keyboard for the same event. We also need to
// send UIPressTypeSelect event to Kabuki from this function.
- (void)sendEvent:(UIEvent*)event {
  if (event.type == UIEventTypePresses) {
    for (UIPress* press in ((UIPressesEvent*)event).allPresses) {
      SbKey key;
      switch (press.type) {
        case UIPressTypeUpArrow:
          key = kSbKeyUp;
          break;
        case UIPressTypeDownArrow:
          key = kSbKeyDown;
          break;
        case UIPressTypeLeftArrow:
          key = kSbKeyLeft;
          break;
        case UIPressTypeRightArrow:
          key = kSbKeyRight;
          break;
        case UIPressTypeSelect:
          key = kSbKeyReturn;
          break;
        default:
          // TODO(b/277641150): Check if volume control key codes can also be
          // tracked.
          key = kSbKeyUnknown;
          break;
      }
      if (press.phase == UIPressPhaseBegan) {
        // Only initiate a keydown if UIPresses should be processed.
        if (_processUIPresses &&
            _processedKeydownPressTypes.count(press.type) == 0) {
          _processedKeydownPressTypes.insert(press.type);
          [_keyboardInputDevice keyPressed:key modifiers:kSbKeyModifiersNone];
        }
      } else if (press.phase == UIPressPhaseEnded ||
                 press.phase == UIPressPhaseCancelled) {
        // Only send a keyup if keydown was sent.
        if (_processedKeydownPressTypes.count(press.type) > 0) {
          _processedKeydownPressTypes.erase(press.type);
          [_keyboardInputDevice keyUnpressed:key modifiers:kSbKeyModifiersNone];
        }
      }
    }
  }
  [super sendEvent:event];
}

- (BOOL)keyboardShowing {
  return _viewController.keyboardShowing;
}

- (BOOL)keyboardFocused {
  return _viewController.keyboardFocused;
}

- (SbWindowRect)getOnScreenKeyboardFrame {
  return [_viewController getOnScreenKeyboardFrame];
}

- (void)focusOnScreenKeyboardWithTicket:(NSInteger)ticket {
  onApplicationMainThread(^{
    if (![self keyboardShowing]) {
      auto eventData = new int;
      *eventData = ticket;
      ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardFocused,
                                     eventData);
      return;
    }
    [self->_viewController focusOnScreenKeyboardWithTicket:ticket];
  });
}

- (void)blurOnScreenKeyboardWithTicket:(NSInteger)ticket {
  onApplicationMainThread(^{
    if (![self keyboardShowing]) {
      auto eventData = new int;
      *eventData = ticket;
      ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardBlurred,
                                     eventData);
      return;
    }
    [self->_viewController blurOnScreenKeyboardWithTicket:ticket];
  });
}

- (void)showOnScreenKeyboardWithText:(NSString*)text ticket:(NSInteger)ticket {
  dispatch_async(dispatch_get_main_queue(), ^{
    [self->_viewController showOnScreenKeyboardWithText:text ticket:ticket];
    SbWindow starboardWindow = [SBDGetApplication().windowManager
        starboardWindowForApplicationWindow:self];
    auto eventData = new SbEventWindowSizeChangedData;
    eventData->window = starboardWindow;
    eventData->size = self.size;
    ApplicationDarwin::InjectEvent(kSbEventTypeWindowSizeChanged, eventData);
  });
}

- (void)hideOnScreenKeyboardWithTicket:(NSInteger)ticket {
  dispatch_async(dispatch_get_main_queue(), ^{
    [self->_viewController hideOnScreenKeyboardWithTicket:ticket];
    SbWindow starboardWindow = [SBDGetApplication().windowManager
        starboardWindowForApplicationWindow:self];
    auto eventData = new SbEventWindowSizeChangedData;
    eventData->window = starboardWindow;
    eventData->size = self.size;
    ApplicationDarwin::InjectEvent(kSbEventTypeWindowSizeChanged, eventData);
  });
}

- (void)onScreenKeyboardKeepFocus:(BOOL)keepFocus {
  onApplicationMainThread(^{
    [self->_viewController onScreenKeyboardKeepFocus:keepFocus];
  });
}

- (void)setOnScreenKeyboardBackgroundColorWithRed:(CGFloat)red
                                            green:(CGFloat)green
                                             blue:(CGFloat)blue
                                            alpha:(CGFloat)alpha {
  onApplicationMainThread(^{
    if ([self keyboardShowing]) {
      [self->_viewController setOnScreenKeyboardBackgroundColorWithRed:red
                                                                 green:green
                                                                  blue:blue
                                                                 alpha:alpha];
    }
  });
}

- (void)setOnScreenKeyboardLightTheme:(BOOL)lightTheme {
  onApplicationMainThread(^{
    if ([self keyboardShowing]) {
      [self->_viewController setOnScreenKeyboardLightTheme:lightTheme];
    }
  });
}

- (void)close {
  if (self.isKeyWindow) {
    [self resignKeyWindow];
  }
  self.hidden = YES;
  self.windowLevel = -1;
}

- (void)attachSurface:(SBDEglSurface*)surface {
  dispatch_async(dispatch_get_main_queue(), ^{
    // Display surfaces are not interactable.
    UIView* view = surface.view;
    view.userInteractionEnabled = NO;
    [self->_viewController.applicationView.interfaceContainer addSubview:view];
    [self->_viewController.applicationView.interfaceContainer
        sendSubviewToBack:view];
  });
}

- (void)attachPlayerView:(UIView*)playerView {
  [_viewController.applicationView.playerContainer addSubview:playerView];
}

- (void)setFocus:(id<UIFocusEnvironment>)focus {
  // In order to change focus, a focus update needs to happen on the
  // environment containing the current focus. This UIWindow is guaranteed to
  // contain anything that is currently focused.
  _focusOverride = focus;
  if (!focus) {
    // Force focus on the interface.
    _focusOverride = [_viewController.applicationView forceDefaultFocus:YES];
  }
  [self setNeedsFocusUpdate];
  [self updateFocusIfNeeded];
  _focusOverride = nil;
  if (!focus) {
    [_viewController.applicationView forceDefaultFocus:NO];
  }
}

@end

@implementation SBDSearchContainerViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_delegate searchDidAppear];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [_delegate searchDidDisappear];
}

@end

@implementation SBDApplicationWindowViewController {
  UISearchController* _searchController;
  SBDSearchResultsViewController* _searchResultsViewController;
  SBDSearchContainerViewController* _searchContainerViewController;
  CGRect _resultsFrameOnShow;
  BOOL _keyboardKeepFocus;
  BOOL _keyboardVisible;
  BOOL _focusWhenVisible;
  NSInteger _delayedFocusTicket;
  NSInteger _lastShowTicket;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    CGRect mainScreenBounds = [UIScreen mainScreen].bounds;
    _applicationView =
        [[SBDApplicationView alloc] initWithFrame:mainScreenBounds];
  }
  return self;
}

- (void)viewDidLoad {
  [self.view addSubview:_applicationView];
}

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  static NSDate* searchResultLastDate;
  static NSString* searchResultLastString;

  if (!_keyboardShowing) {
    return;
  }
  searchResultLastDate = [NSDate date];

  // Debouncing searchResults to avoid sending too many intermediate input
  // events to Kabuki and a bug where voice search queries are cleared.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(kSearchResultDebounceTime * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        if (-searchResultLastDate.timeIntervalSinceNow <=
            kSearchResultDebounceTime) {
          return;
        }
        NSString* searchString = searchController.searchBar.text;
        if (searchResultLastString &&
            [searchResultLastString isEqualToString:searchString]) {
          return;
        }
        searchResultLastString = [searchString copy];
        SBDWindowManager* windowManager = SBDGetApplication().windowManager;
        [windowManager.currentApplicationWindow.keyboardInputDevice
            onScreenKeyboardTextEntered:searchString];
      });
}

- (SbWindowRect)getOnScreenKeyboardFrame {
  if (!_keyboardVisible) {
    return {0, 0, 0, 0};
  }

  // UISearchControl has three different keyboard layouts:

  // case 1: Siri remote.
  // +-------------------------------+
  // | Searchbar (shows search text) |
  // +-------------------------------+
  // |           Keyboard            |
  // +-------------------------------+
  // |        Search Results         |
  // |             ...               |
  // +-------------------------------+

  // case 2: Non-Siri remote, with LTR language in device's general
  // setting.
  // +-------------------------------+
  // | Searchbar (shows search text) |
  // +--------------+----------------+
  // |   Keyboard   |     Search     |
  // |              |     Results    |
  // |     ...      |       ...      |
  // +--------------+----------------+

  // case 3: Non-Siri remote, with RTL language in device's general
  // setting.
  // +-------------------------------+
  // | Searchbar (shows search text) |
  // +--------------+----------------+
  // |   Search     |     Keyboard   |
  // |   Results    |                |
  // |     ...      |       ...      |
  // +--------------+----------------+

  // Infer the keyboard frame based on the search result's frame.
  CGSize screenSize = UIScreen.mainScreen.bounds.size;
  CGRect keyboardFrame;
  CGFloat resultsFrameLeftPadding = _resultsFrameOnShow.origin.x;
  CGFloat resultsFrameRightPadding = screenSize.width -
                                     _resultsFrameOnShow.origin.x -
                                     _resultsFrameOnShow.size.width;

  if (_resultsFrameOnShow.origin.x == 0) {
    // This is the horizontal keyboard layout illustrated in case 1.
    // Specify keyboard frame as including keyboard and search bar.
    keyboardFrame =
        CGRectMake(0, 0, screenSize.width, _resultsFrameOnShow.origin.y);
  } else {
    if (resultsFrameLeftPadding > resultsFrameRightPadding) {
      // This is the LTR grid keyboard layout illustrated in case 2.
      // Specify keyboard frame as only the keyboard area. This allows the
      // caller to infer the space taken up by the search bar.
      keyboardFrame = CGRectMake(
          0, _resultsFrameOnShow.origin.y, _resultsFrameOnShow.origin.x,
          screenSize.height - _resultsFrameOnShow.origin.y);
    } else {
      // This is the RTL grid keyboard layout illustrated in case 3.
      // Specify keyboard frame as only the keyboard area. This allows the
      // caller to infer the space taken up by the search bar.
      keyboardFrame = CGRectMake(
          _resultsFrameOnShow.origin.x + _resultsFrameOnShow.size.width,
          _resultsFrameOnShow.origin.y,
          screenSize.width - _resultsFrameOnShow.size.width -
              _resultsFrameOnShow.origin.x,
          screenSize.height - _resultsFrameOnShow.origin.y);
    }
  }

  // Convert from points to pixels.
  CGFloat scale = UIScreen.mainScreen.scale;
  return {static_cast<float>(keyboardFrame.origin.x * scale),
          static_cast<float>(keyboardFrame.origin.y * scale),
          static_cast<float>(keyboardFrame.size.width * scale),
          static_cast<float>(keyboardFrame.size.height * scale)};
}

- (void)showOnScreenKeyboardWithText:(NSString*)text ticket:(NSInteger)ticket {
  if (_keyboardShowing) {
    // For some languages such as Hebrew, setting the search bar text to the
    // same value may result in the text being cleared. Work around this bug.
    if (![_searchController.searchBar.text isEqualToString:text]) {
      _searchController.searchBar.text = text;
    }
    auto eventData = new int;
    *eventData = ticket;
    ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardShown,
                                   eventData);
    return;
  }
  if (_searchController.isBeingPresented ||
      _searchController.isBeingDismissed) {
    __weak SBDApplicationWindowViewController* weakself = self;
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          SBDApplicationWindowViewController* strongself = weakself;
          if (!strongself) {
            return;
          }
          [strongself showOnScreenKeyboardWithText:text ticket:ticket];
        });
    return;
  }

  _keyboardShowing = YES;
  _lastShowTicket = ticket;

  _searchResultsViewController = [[SBDSearchResultsViewController alloc] init];
  _searchResultsViewController.focusDelegate = self;
  _searchController = [[UISearchController alloc]
      initWithSearchResultsController:_searchResultsViewController];
  _searchController.searchBar.text = text;
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.hidesNavigationBarDuringPresentation = NO;
  _searchController.delegate = self;
  _searchController.searchResultsUpdater = self;

  _searchContainerViewController = [[SBDSearchContainerViewController alloc]
      initWithSearchController:_searchController];
  _searchContainerViewController.delegate = self;

  [self addChildViewController:_searchContainerViewController];
  [_searchContainerViewController didMoveToParentViewController:self];
  [_applicationView.searchContainer
      addSubview:_searchContainerViewController.view];
  _searchController.active = YES;

  // Set background colors as requested in info.plist.
  // Search controls should use YTOSKBackgroundColor if it exists.
  NSString* colorFromPList = [[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"YTOSKBackgroundColor"];
  if (!colorFromPList) {
    colorFromPList = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"YTApplicationBackgroundColor"];
  }
  NSArray<NSString*>* colorComponents =
      [colorFromPList componentsSeparatedByString:@", "];
  if (colorComponents.count == 4) {
    _searchController.view.backgroundColor =
        [UIColor colorWithRed:[colorComponents[0] floatValue] / 255.0
                        green:[colorComponents[1] floatValue] / 255.0
                         blue:[colorComponents[2] floatValue] / 255.0
                        alpha:[colorComponents[3] floatValue]];
  } else {
    _searchController.view.backgroundColor = [UIColor colorWithRed:30 / 255.0
                                                             green:30 / 255.0
                                                              blue:30 / 255.0
                                                             alpha:1];
  }
  // Search results should use the application window color.
  _searchResultsViewController.view.backgroundColor =
      [SBDApplicationWindow defaultWindowColor];
}

- (void)setOnScreenKeyboardBackgroundColorWithRed:(CGFloat)red
                                            green:(CGFloat)green
                                             blue:(CGFloat)blue
                                            alpha:(CGFloat)alpha {
  _searchController.view.backgroundColor = [UIColor colorWithRed:red
                                                           green:green
                                                            blue:blue
                                                           alpha:alpha];
}

- (void)setOnScreenKeyboardLightTheme:(BOOL)lightTheme {
  if (lightTheme) {
    _searchController.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
  } else {
    _searchController.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
}

- (void)hideOnScreenKeyboardWithTicket:(NSInteger)ticket {
  if (!_keyboardShowing) {
    auto eventData = new int;
    *eventData = ticket;
    ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardHidden,
                                   eventData);
    return;
  }
  if (_searchController.isBeingPresented ||
      _searchController.isBeingDismissed) {
    __weak SBDApplicationWindowViewController* weakself = self;
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          SBDApplicationWindowViewController* strongself = weakself;
          if (!strongself) {
            return;
          }
          [strongself hideOnScreenKeyboardWithTicket:ticket];
        });
    return;
  }
  _keyboardShowing = NO;
  [_searchContainerViewController.view removeFromSuperview];
  _searchController.active = NO;
  [_searchContainerViewController willMoveToParentViewController:nil];
  [_searchContainerViewController removeFromParentViewController];

  auto eventData = new int;
  *eventData = ticket;
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardHidden, eventData);
}

- (void)onScreenKeyboardKeepFocus:(BOOL)keepFocus {
  _keyboardKeepFocus = keepFocus;
  if (!_keyboardShowing) {
    return;
  }
  _searchResultsViewController.view.userInteractionEnabled = !keepFocus;
}

- (void)focusOnScreenKeyboardWithTicket:(NSInteger)ticket {
  if (!_keyboardVisible) {
    _focusWhenVisible = YES;
    _delayedFocusTicket = ticket;
    return;
  }
  _keyboardFocused = YES;

  auto eventData = new int;
  *eventData = ticket;
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardFocused,
                                 eventData);
}

- (void)blurOnScreenKeyboardWithTicket:(NSInteger)ticket {
  _keyboardFocused = NO;

  auto eventData = new int;
  *eventData = ticket;
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardBlurred,
                                 eventData);
}

#pragma mark - SBDSearchResultsFocusDelegate

- (void)resultsDidLoseFocus {
  _keyboardFocused = YES;
  auto eventData = new int(kSbEventOnScreenKeyboardInvalidTicket);
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardFocused,
                                 eventData);
}

- (void)resultsDidReceiveFocus {
  _keyboardFocused = NO;
  auto eventData = new int(kSbEventOnScreenKeyboardInvalidTicket);
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardBlurred,
                                 eventData);
}

#pragma mark - SBDSearchViewDisplayDelegate

- (void)searchDidDisappear {
  // Move the interface container back into the application view.
  // NOTE: addSubview will remove the target from its superview, so don't
  //   removeFromSuperview explicitly as that may result in a focus change.
  [_applicationView addSubview:_applicationView.interfaceContainer];

  _searchResultsViewController = nil;
  _keyboardFocused = NO;
  _keyboardVisible = NO;
  _focusWhenVisible = NO;
}

- (void)searchDidAppear {
  UIView* resultsView = _searchResultsViewController.view;
  _resultsFrameOnShow =
      [resultsView convertRect:resultsView.frame
             toCoordinateSpace:UIScreen.mainScreen.coordinateSpace];

  // Move the interface container into the search results controller to allow
  // seamless transitions between the search interface and our application's
  // interface.
  [_searchResultsViewController.view
      addSubview:_applicationView.interfaceContainer];
  _searchResultsViewController.view.userInteractionEnabled =
      !_keyboardKeepFocus;
  _keyboardVisible = YES;

  auto eventData = new int;
  *eventData = _lastShowTicket;
  ApplicationDarwin::InjectEvent(kSbEventTypeOnScreenKeyboardShown, eventData);

  if (_focusWhenVisible) {
    _focusWhenVisible = NO;
    [self focusOnScreenKeyboardWithTicket:_delayedFocusTicket];
  }
}

@end
