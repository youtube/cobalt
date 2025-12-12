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

#ifndef STARBOARD_TVOS_SHARED_INPUT_EVENTS_H_
#define STARBOARD_TVOS_SHARED_INPUT_EVENTS_H_

#import <Foundation/Foundation.h>

#include "starboard/key.h"
#include "starboard/window.h"

/**
 *  @brief A utility class to send input events to the application.
 *  @seealso starboard/event.h
 */
@interface SBDInputEvents : NSObject

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputDeviceTypeOnScreenKeyboard.
 *  @param text The text which was entered into the on screen keyboard.
 *  @param window The window the key should be delivered to.
 */
+ (void)onScreenKeyboardTextUpdated:(NSString*)text window:(SbWindow)window;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypePress.
 *  @param key The key that was pressed.
 *      @seealso starboard/key.h
 *  @param modifiers Modifiers (ctrl, shift, etc) associated with the event.
 *      @seealso starboard/key.h
 *  @param window The window the key should be delivered to.
 *  @param deviceID The unique identifier of the controller the key press
 *      occurred on.
 */
+ (void)keyPressedEventWithKey:(SbKey)key
                     modifiers:(SbKeyModifiers)modifiers
                        window:(SbWindow)window
                      deviceID:(NSInteger)deviceID;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypeUnpress.
 *  @param key The key that became unpressed.
 *      @seealso starboard/key.h
 *  @param modifiers Modifiers (ctrl, shift, etc) associated with the event.
 *      @seealso starboard/key.h
 *  @param window The window the key should be delivered to.
 *  @param deviceID The unique identifier of the controller the key press
 *      occurred on.
 */
+ (void)keyUnpressedEventWithKey:(SbKey)key
                       modifiers:(SbKeyModifiers)modifiers
                          window:(SbWindow)window
                        deviceID:(NSInteger)deviceID;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypeMove.
 *  @param key The key that performed the movement.
 *      @seealso starboard/key.h
 *  @param window The window the key should be delivered to.
 *  @param xPosition The x position of the key.
 *  @param yPosition The y position of the key.
 *  @param deviceID The unique identifier of the controller the key press
 *      occurred on.
 */
+ (void)moveEventWithKey:(SbKey)key
                  window:(SbWindow)window
               xPosition:(float)xPosition
               yPosition:(float)yPosition
                deviceID:(NSInteger)deviceID;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypePress.
 *  @param window The window the touch should be delivered to.
 *  @param xPosition The x position of the touch.
 *  @param yPosition The y position of the touch.
 *  @param deviceID The unique identifier of the associated controller.
 */
+ (void)touchpadDownEventWithWindow:(SbWindow)window
                          xPosition:(float)xPosition
                          yPosition:(float)yPosition
                           deviceID:(NSInteger)deviceID;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypePress.
 *  @param window The window the touch should be delivered to.
 *  @param xPosition The last x position of the touch.
 *  @param yPosition The last y position of the touch.
 *  @param deviceID The unique identifier of the associated controller.
 */
+ (void)touchpadUpEventWithWindow:(SbWindow)window
                        xPosition:(float)xPosition
                        yPosition:(float)yPosition
                         deviceID:(NSInteger)deviceID;

/**
 *  @brief Injects an event which represents a @c kSbEventTypeInput event
 *      with the @c SbInputData data argument having @c SbInputEventType
 *      @c kSbInputEventTypeMove.
 *  @param key The key that performed the movement.
 *      @seealso starboard/key.h
 *  @param window The window the key should be delivered to.
 *  @param xPosition The x position of the key.
 *  @param yPosition The y position of the key.
 *  @param deviceID The unique identifier of the controller the key press
 *      occurred on.
 */
+ (void)touchpadMoveEventWithWindow:(SbWindow)window
                          xPosition:(float)xPosition
                          yPosition:(float)yPosition
                           deviceID:(NSInteger)deviceID;

@end

#endif  // STARBOARD_TVOS_SHARED_INPUT_EVENTS_H_
