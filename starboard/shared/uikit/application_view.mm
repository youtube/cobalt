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

#import "starboard/shared/uikit/application_view.h"

#import <GameController/GameController.h>

#import "starboard/shared/uikit/defines.h"
#import "starboard/shared/uikit/game_controller_input_device.h"
#import "starboard/shared/uikit/starboard_application.h"
#import "starboard/shared/uikit/trackpad_input_device.h"
#import "starboard/shared/uikit/window_manager.h"

/**
 *  @brief SBDApplicationViewInterfaceContainer is specifically a UIScrollView.
 *    When attached as a search results subview, this makes UISearchController
 *    automatically hide the keyboard when the results are focused.
 */
@interface SBDApplicationViewInterfaceContainer : UIScrollView
- (instancetype)initWithFrame:(CGRect)frame responder:(UIResponder*)responder;
@end

@implementation SBDApplicationViewInterfaceContainer {
  UIResponder* _responder;
}

- (instancetype)initWithFrame:(CGRect)frame responder:(UIResponder*)responder {
  self = [super initWithFrame:frame];
  if (self) {
    // Set the content size to be slightly larger than the frame so that
    // UISearchController will auto-dismiss the keyboard control.
    self.contentSize = CGSizeMake(frame.size.width + 1, frame.size.height + 1);
    self.scrollEnabled = NO;
    _responder = responder;
  }
  return self;
}

- (UIResponder*)nextResponder {
  return _responder;
}
@end

/**
 *  @brief SBDApplicationViewInterfaceFocus is a custom control that is
 *    focusable only if no sibling views are interactable. This allows the
 *    control to act as a placeholder focus if UI navigation is not used.
 */
@interface SBDApplicationViewInterfaceFocus : UIControl
@property(nonatomic) BOOL canBecomeFocusedOverride;
@end

@implementation SBDApplicationViewInterfaceFocus
- (BOOL)canBecomeFocused {
  if (_canBecomeFocusedOverride) {
    return YES;
  }
  for (UIView* view in self.superview.subviews) {
    // Any interactable siblings should get focus.
    if (view == self) {
      continue;
    }
    if (view.userInteractionEnabled) {
      return NO;
    }
  }
  return YES;
}

- (BOOL)isAccessibilityElement {
  return self.canBecomeFocused;
}

- (UIAccessibilityTraits)accessibilityTraits {
  return UIAccessibilityTraitAllowsDirectInteraction;
}

- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext*)context {
  // Keep focus unless moving to a UI navigation item. Otherwise, UIKit will
  // change focus to other UISearchController items. The client app will
  // manually set focus in this situation.
  if (context.previouslyFocusedItem == self) {
    return !self.canBecomeFocused;
  }
  return YES;
}
@end

@implementation SBDApplicationView {
  /**
   *  @brief We always attach a trackpad to send trackpad events to.
   */
  SBDTrackpadInputDevice* _trackpad;

  /**
   *  @brief A set of the connected controllers we're aware of.
   */
  NSMutableSet<SBDGameControllerInputDevice*>* _gameControllers;

  /**
   *  @brief Whether the search view is currently focused.
   */
  BOOL _searchFocused;

  /**
   *  @brief This is the default focus view.
   */
  SBDApplicationViewInterfaceFocus* _interfaceFocus;
}

- (instancetype)init SBD_UNAVAILABLE_INITIALIZER_IMPL;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _gameControllers = [NSMutableSet set];

    _searchContainer = [[UIView alloc] initWithFrame:self.bounds];
    _searchContainer.accessibilityIdentifier = @"Search Container";
    [self addSubview:_searchContainer];

    _playerContainer = [[UIView alloc] initWithFrame:frame];
    _playerContainer.accessibilityIdentifier = @"Player Container";
    _playerContainer.backgroundColor = [UIColor blackColor];
    [self addSubview:_playerContainer];

    _interfaceContainer =
        [[SBDApplicationViewInterfaceContainer alloc] initWithFrame:frame
                                                          responder:self];
    _interfaceContainer.accessibilityIdentifier = @"Interface Container";
    [self addSubview:_interfaceContainer];

    _interfaceFocus = [[SBDApplicationViewInterfaceFocus alloc]
        initWithFrame:CGRectMake(1, 1, 1, 1)];
    [_interfaceContainer addSubview:_interfaceFocus];

    _trackpad = [[SBDTrackpadInputDevice alloc] init];

    NSNotificationCenter* notifications = [NSNotificationCenter defaultCenter];
    [notifications addObserver:self
                      selector:@selector(controllerConnected:)
                          name:GCControllerDidConnectNotification
                        object:nil];
    [notifications addObserver:self
                      selector:@selector(controllerDisconnected:)
                          name:GCControllerDidDisconnectNotification
                        object:nil];

    dispatch_async(dispatch_get_main_queue(), ^{
      [self addExistingControllers];

      // Activate this as the first responder. This ensures that keyCommands
      // is queried and hardware keyboard input is processed.
      [self becomeFirstResponder];
    });
  }
  return self;
}

- (NSArray<id<UIFocusEnvironment>>*)preferredFocusEnvironments {
  if (_searchFocused) {
    return @[ _searchContainer ];
  }
  return @[ _interfaceContainer, self ];
}

- (void)dealloc {
  NSNotificationCenter* notifications = [NSNotificationCenter defaultCenter];
  [notifications removeObserver:self
                           name:GCControllerDidConnectNotification
                         object:nil];
  [notifications removeObserver:self
                           name:GCControllerDidDisconnectNotification
                         object:nil];
}

- (BOOL)isAccessibilityElement {
  return self.canBecomeFocused;
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (UIAccessibilityTraits)accessibilityTraits {
  return UIAccessibilityTraitAllowsDirectInteraction;
}

- (UIView*)forceDefaultFocus:(BOOL)forceFocus {
  _interfaceFocus.canBecomeFocusedOverride = forceFocus;
  return _interfaceFocus;
}

#pragma mark - Controller Connect/Disconnect Notifications

- (void)controllerConnected:(NSNotification*)notification {
  GCController* controller = (GCController*)notification.object;
  [self startListeningToInputForController:controller];
}

- (void)controllerDisconnected:(NSNotification*)notification {
  GCController* controller = (GCController*)notification.object;
  [self stopListeningToInputForController:controller];
}

#pragma mark - Controller Connect/Disconnect

/**
 *  @brief Adds controllers that are already connected to the system.
 */
- (void)addExistingControllers {
  NSArray<GCController*>* controllers = [GCController controllers];
  for (GCController* controller in controllers) {
    [self startListeningToInputForController:controller];
  }
}

/**
 *  @brief Starts listening to inputs from the given controller.
 *  @param controller The controller to start listening for input events.
 */
- (void)startListeningToInputForController:(GCController*)controller {
  for (SBDGameControllerInputDevice* gameController in _gameControllers) {
    if ([gameController.controller isEqual:controller]) {
      return;
    }
  }

  SBDGameControllerInputDevice* gameController =
      [[SBDGameControllerInputDevice alloc] initWithGameController:controller];
  [_gameControllers addObject:gameController];

  if (controller.microGamepad) {
    [self addDpadHandler:controller];
    return;
  }

  if (controller.extendedGamepad) {
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    [gameController bindButton:gamepad.dpad.left toKey:kSbKeyGamepadDPadLeft];
    [gameController bindButton:gamepad.dpad.right toKey:kSbKeyGamepadDPadRight];
    [gameController bindButton:gamepad.dpad.up toKey:kSbKeyGamepadDPadUp];
    [gameController bindButton:gamepad.dpad.down toKey:kSbKeyGamepadDPadDown];
    [gameController bindLeftThumbstick:gamepad.leftThumbstick];
    [gameController bindRightThumbstick:gamepad.rightThumbstick];
    [gameController bindButton:gamepad.buttonA toKey:kSbKeyGamepad1];
    [gameController bindButton:gamepad.buttonB toKey:kSbKeyGamepad2];
    [gameController bindButton:gamepad.buttonX toKey:kSbKeyGamepad3];
    [gameController bindButton:gamepad.buttonY toKey:kSbKeyGamepad4];
    [gameController bindButton:gamepad.leftShoulder
                         toKey:kSbKeyGamepadLeftBumper];
    [gameController bindButton:gamepad.rightShoulder
                         toKey:kSbKeyGamepadRightBumper];
    return;
  }

  if (controller.gamepad) {
    GCGamepad* gamepad = controller.gamepad;
    [gameController bindButton:gamepad.dpad.left toKey:kSbKeyGamepadDPadLeft];
    [gameController bindButton:gamepad.dpad.right toKey:kSbKeyGamepadDPadRight];
    [gameController bindButton:gamepad.dpad.up toKey:kSbKeyGamepadDPadUp];
    [gameController bindButton:gamepad.dpad.down toKey:kSbKeyGamepadDPadDown];
    [gameController bindButton:gamepad.buttonA toKey:kSbKeyGamepad1];
    [gameController bindButton:gamepad.buttonB toKey:kSbKeyGamepad2];
    [gameController bindButton:gamepad.buttonX toKey:kSbKeyGamepad3];
    [gameController bindButton:gamepad.buttonY toKey:kSbKeyGamepad4];
    [gameController bindButton:gamepad.leftShoulder
                         toKey:kSbKeyGamepadLeftBumper];
    [gameController bindButton:gamepad.rightShoulder
                         toKey:kSbKeyGamepadRightBumper];
    return;
  }
}

- (void)addDpadHandler:(GCController*)controller {
  if (controller.extendedGamepad || controller.gamepad) {
    // Only handle Siri remotes here.
    return;
  }
  static CGPoint _lastSiriRemoteGamepadLocation;
  static BOOL _isBeingTouched;
  __weak GCMicroGamepad* siriRemoteGamepad = controller.microGamepad;
  if (!siriRemoteGamepad) {
    return;
  }
  siriRemoteGamepad = siriRemoteGamepad;
  siriRemoteGamepad.reportsAbsoluteDpadValues = YES;
  siriRemoteGamepad.valueChangedHandler = ^(GCMicroGamepad* gamepad,
                                            GCControllerElement* element) {
    if (element != gamepad.dpad) {
      return;
    }
    if (_searchFocused) {
      return;
    }
    GCControllerDirectionPad* dpad = gamepad.dpad;
    CGPoint newLocation = CGPointMake(dpad.xAxis.value, -dpad.yAxis.value);
    BOOL isBeingTouched = dpad.up.pressed || dpad.down.pressed ||
                          dpad.left.pressed || dpad.right.pressed;
    if (!_isBeingTouched && isBeingTouched) {
      // If the dpad has gone from not being touched, to being touched,
      // report a touch down.
      [_trackpad
          touchDownAtPosition:SBDVectorMake(newLocation.x, newLocation.y)];
    } else if (isBeingTouched) {
      [_trackpad moveToPosition:SBDVectorMake(newLocation.x, newLocation.y)];
    } else {
      // No longer being touched, report a touch up.
      [_trackpad
          touchUpAtPosition:SBDVectorMake(_lastSiriRemoteGamepadLocation.x,
                                          _lastSiriRemoteGamepadLocation.y)];
    }
    _lastSiriRemoteGamepadLocation = newLocation;
    _isBeingTouched = isBeingTouched;
  };
}

/**
 *  @brief Stops listening to inputs from the given controller.
 *  @param controller The controller to stop listening for input events.
 */
- (void)stopListeningToInputForController:(GCController*)controller {
  NSMutableArray<SBDGameControllerInputDevice*>* controllersToRemove =
      [NSMutableArray array];
  for (SBDGameControllerInputDevice* gameController in _gameControllers) {
    if ([gameController.controller isEqual:controller]) {
      [controllersToRemove addObject:gameController];
    }
  }
  for (SBDGameControllerInputDevice* gameController in controllersToRemove) {
    [_gameControllers removeObject:gameController];
    [gameController disconnect];
  }
}

#pragma mark - SBDSearchControllerFocusDelegate

- (void)searchShouldFocus {
  if (_searchFocused) {
    return;
  }
  _searchFocused = YES;
  // Since the _interfaceContainer can be in the _searchContainer's hierarchy,
  // disable interactions with it to avoid accidentally focusing it.
  BOOL wasEnabled = _interfaceContainer.userInteractionEnabled;
  _interfaceContainer.userInteractionEnabled = NO;
  SBDWindowManager* windowManager = SBDGetApplication().windowManager;
  [windowManager.currentApplicationWindow setFocus:_searchContainer];
  _interfaceContainer.userInteractionEnabled = wasEnabled;
}

- (void)searchDidFocus {
  _searchFocused = YES;
}

- (void)searchShouldBlur {
  if (!_searchFocused) {
    return;
  }
  _searchFocused = NO;
  SBDWindowManager* windowManager = SBDGetApplication().windowManager;
  [windowManager.currentApplicationWindow setFocus:_interfaceContainer];
}

- (void)searchDidBlur {
  _searchFocused = NO;
}

- (void)searchDidShow {
  _playerContainer.backgroundColor = [UIColor clearColor];
  // The @c _searchContainer must be the front-most view or UIKit will prevent
  // the focus to move between the keyboard and the search results.
  [self bringSubviewToFront:_searchContainer];

  // Start with the search controls focused.
  SBDWindowManager* windowManager = SBDGetApplication().windowManager;
  [windowManager.currentApplicationWindow setFocus:_searchContainer];
}

- (void)searchDidHide {
  _playerContainer.backgroundColor = [UIColor blackColor];
  // The @c _searchContainer is sent to the back because the keyboard is still
  // briefly shown on screen, even after receiving that the viewDidDisappear.
  // Sending the view to the back prevents the keyboard from being briefly shown
  // over the top of the GLKView.
  [self sendSubviewToBack:_searchContainer];
  [self searchShouldBlur];
}

@end
