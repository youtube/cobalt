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

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include <atomic>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/event.h"
#import "starboard/tvos/shared/application_darwin.h"
#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#import "starboard/tvos/shared/media/egl_adapter.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/uikit.h"
#import "starboard/tvos/shared/window_manager.h"
#include "starboard/window.h"

using starboard::ApplicationDarwin;

namespace {

int g_main_argc_ = 0;
char** g_main_argv_ = nullptr;
int64_t app_start_time_stamp = 0;

std::atomic_int32_t g_suspend_event_counter_{0};

/**
 *  @brief Pointer to the starboard application.
 */
__weak id<SBDStarboardApplication> g_starboard_application_ = nil;
dispatch_semaphore_t g_applicationSemaphore_;

void DeleteStartData(void* data) {
  SbEventStartData* startData = static_cast<SbEventStartData*>(data);
  delete[] startData->link;
  delete startData;
}

void SuspendDone(void* context) {
  dispatch_async(dispatch_get_main_queue(), ^{
    g_suspend_event_counter_--;
  });
}

}  // namespace

/**
 *  @brief Called when the OS sends a search/play AppIntent.
 */
void SBProcessAppIntent(const char* query, int isSearch) {
  if (query == NULL) {
    SB_LOG(ERROR) << "AppIntent query is null, ignoring.";
    return;
  }
  NSString* urlQuery = [NSString stringWithUTF8String:query];

  NSString* urlBase =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"YTApplicationURL"];
  if (urlBase == NULL) {
    SB_LOG(ERROR) << "YTApplicationURL not in plist, ignoring AppIntent.";
    return;
  }

  NSString* urlAction = (isSearch != 0) ? @"?launch=voice&vs=11&va=search&vaa="
                                        : @"?launch=voice&vs=11&va=play&vaa=";

  NSString* url =
      [NSString stringWithFormat:@"%@%@%@", urlBase, urlAction, urlQuery];
  int urlLength = [url lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

  char* deeplink = new char[urlLength + 1];
  memcpy(deeplink, [url UTF8String], urlLength);

  SB_LOG(INFO) << "Injecting AppIntent deeplink: " << deeplink;
  ApplicationDarwin::InjectEvent(kSbEventTypeLink, deeplink);
}

int SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
  ApplicationDarwin starboardApplication(callback);
  dispatch_semaphore_signal(g_applicationSemaphore_);
  return starboardApplication.Run(0, NULL);
}

/**
 *  @brief The main entry point for the host application.
 */
int SBDUIKitApplicationMain(int argc, char* argv[]) {
  g_main_argc_ = argc;
  g_main_argv_ = argv;
  app_start_time_stamp = starboard::CurrentMonotonicTime();

  @autoreleasepool {
    return UIApplicationMain(argc, argv, @"SBDUIKitApplication", nil);
  }
}

/**
 *  @brief Global accessor for SBDStarboardApplication.
 */
id<SBDStarboardApplication> SBDGetApplication() {
  return g_starboard_application_;
}

/**
 *  @brief This class implements the SBDStarboardApplication. While
 *      ApplicationDarwin is used solely to pass SbEvents, this class
 *      interfaces with the Objective C modules to handle the rest of
 *      the application logic.
 */
@interface SBDUIKitApplication
    : UIApplication <SBDStarboardApplication, UIApplicationDelegate>
@end

@implementation SBDUIKitApplication {
  /**
   *  @brief The dispatch queue on which the starboard application runs.
   *  @remarks Since Starboard starts running and doesn't stop until the
   *      Starboard application quits, this queue is directly related to the
   *      thread on which the Starboard app's main run loop is running.
   */
  dispatch_queue_t _starboardThread;

  /**
   *  @brief The last "menu" press received.
   */
  UIPress* _lastMenuPress;

  /**
   *  @brief The last press event received with @c _lastMenuPress
   */
  UIPressesEvent* _lastMenuPressEvent;

  /**
   *  @brief The last "menu" press ended received.
   */
  UIPress* _lastMenuPressEnd;

  /**
   *  @brief The last press ended event received with @c _lastMenuPressEnd
   */
  UIPressesEvent* _lastMenuPressEventEnd;

  /**
   *  @brief It indicates if the VoiceOver is on.
   */
  BOOL _isVoiceOverRunning;

  /**
   *  @brief It's used to calculate display refresh rate.
   */
  CADisplayLink* _displayLink;
  std::atomic<double> _lastDisplayRefreshRate;
}

@synthesize drmManager = _drmManager;
@synthesize eglAdapter = _eglAdapter;
@synthesize playerManager = _playerManager;
@synthesize windowManager = _windowManager;

#pragma mark - Initialization

- (instancetype)init {
  self = [super init];
  if (self) {
    g_starboard_application_ = self;

    // Set self to handle UIApplicationDelegate messages. The "self.delegate"
    // property is not a strong reference, so this does not introduce a
    // circular reference.
    self.delegate = self;

    _drmManager = [[SBDDrmManager alloc] init];
    _windowManager = [[SBDWindowManager alloc] init];
    _playerManager = [[SBDPlayerManager alloc] init];
    _eglAdapter = [[SBDEglAdapter alloc] init];

    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback
                                           error:nil];

    // The ApplicationDarwin instance must be constructed in the thread on
    // which it will run.
    g_applicationSemaphore_ = dispatch_semaphore_create(0);
    _starboardThread = dispatch_queue_create("starboard", 0);
    dispatch_async(_starboardThread, ^{
      @autoreleasepool {
        [NSThread currentThread].name = @"Starboard";
        int unused_value = 0;
        SbRunStarboardMain(unused_value, nullptr, SbEventHandle);
      }
    });

    // Wait for the application to be started.
    dispatch_semaphore_wait(g_applicationSemaphore_, DISPATCH_TIME_FOREVER);

    // Listen to VoiceOver setting change notification.
    _isVoiceOverRunning = UIAccessibilityIsVoiceOverRunning();
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverStatusChanged)
               name:UIAccessibilityVoiceOverStatusChanged
             object:nil];

    // Create a display link and add to current loop.
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(display:)];
    [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (void)display:(CADisplayLink*)sender {
  // Calculate the actual frame rate.
  double interval = sender.targetTimestamp - sender.timestamp;
  if (interval > 0) {
    _lastDisplayRefreshRate = 1.0 / interval;
  }
}

- (double)displayRefreshRate {
  return _lastDisplayRefreshRate;
}

#pragma mark - UIResponder

- (void)pressesBegan:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  _lastMenuPress = nil;
  _lastMenuPressEvent = nil;
  for (UIPress* press in presses) {
    if (press.type == UIPressTypeMenu) {
      // Intercept UIPressTypeMenu event and do not forward it to the
      // superclass's pressesBegan method, as it will cause the application to
      // exit immediately. Instead, we save the UIPressTypeMenu press and event
      // in _lastMenuPress and _lastMenuPressEvent, and only forward it when
      // suspendApplication is invoked.
      _lastMenuPress = press;
      _lastMenuPressEvent = event;
      return;
    }
  }
  [super pressesBegan:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  _lastMenuPressEnd = nil;
  _lastMenuPressEventEnd = nil;
  for (UIPress* press in presses) {
    if (press.type == UIPressTypeMenu) {
      _lastMenuPressEnd = press;
      _lastMenuPressEventEnd = event;
      return;
    }
  }
  [super pressesEnded:presses withEvent:event];
}

#pragma mark - SBDStarboardApplication

- (void)suspendApplication {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (_lastMenuPress && _lastMenuPressEvent && _lastMenuPressEnd &&
        _lastMenuPressEventEnd) {
      NSSet<UIPress*>* menuPress = [NSSet setWithObject:_lastMenuPress];
      [super pressesBegan:menuPress withEvent:_lastMenuPressEvent];
      NSSet<UIPress*>* menuPressEnd = [NSSet setWithObject:_lastMenuPressEnd];
      [super pressesEnded:menuPressEnd withEvent:_lastMenuPressEventEnd];
    }
  });
}

#pragma mark - UIApplicationDelegate

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  char* linkString = nullptr;
  NSURL* startupUrl = launchOptions[UIApplicationLaunchOptionsURLKey];
  NSString* urlString = startupUrl.absoluteString;
  if (urlString) {
    NSUInteger urlStringLength =
        [urlString lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    linkString = new char[urlStringLength + 1];
    [urlString getCString:linkString
                maxLength:urlStringLength + 1
                 encoding:NSUTF8StringEncoding];
  }

  auto eventData = new SbEventStartData;
  eventData->link = linkString;
  eventData->argument_count = g_main_argc_;
  eventData->argument_values = g_main_argv_;
  ApplicationDarwin::InjectEvent(kSbEventTypeStart, app_start_time_stamp,
                                 eventData, &DeleteStartData);
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
  ApplicationDarwin::Get()->Blur(nullptr, nullptr);
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  ApplicationDarwin::Get()->Focus(nullptr, nullptr);
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
  g_suspend_event_counter_++;
  ApplicationDarwin::Get()->Freeze(NULL, &SuspendDone);

  // Wait for the application to finish processing the suspend event.
  while (g_suspend_event_counter_) {
    // Run runloop until it processes an event or until timeout. SuspendDone()
    // sends a block to the main thread, it would be executed on main thread and
    // wake up this while loop. After the block is executed, the loop would quit
    // immediately.
    const double kTimeoutInSeconds = 3;
    CFRunLoopRunResult result =
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, kTimeoutInSeconds, true);
    // If the suspend processing exceeds the allowed processing time, it may
    // reach the 3s timeout when the app is resumed. But SuspendDone() should
    // be called eventually and decreases |g_suspend_event_counter_|.
    if (result == kCFRunLoopRunTimedOut) {
      SB_LOG(ERROR) << "Suspending application takes too long time.";
    }
  }
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
  _isVoiceOverRunning = UIAccessibilityIsVoiceOverRunning();
  ApplicationDarwin::Get()->Reveal(nullptr, nullptr);
}

- (void)applicationWillTerminate:(UIApplication*)application {
  ApplicationDarwin::Get()->Stop(0);
}

- (BOOL)application:(UIApplication*)app
            openURL:(NSURL*)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id>*)options {
  NSString* urlString = url.absoluteString;
  NSUInteger urlStringLength =
      [urlString lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  char* linkData = new char[urlStringLength + 1];
  [urlString getCString:linkData
              maxLength:urlStringLength + 1
               encoding:NSUTF8StringEncoding];
  ApplicationDarwin::Get()->Link(linkData);
  delete[] linkData;
  return YES;
}

#pragma mark - VoiceOverNotification

- (void)voiceOverStatusChanged {
  BOOL isVoiceOverRunning = UIAccessibilityIsVoiceOverRunning();
  if (isVoiceOverRunning != _isVoiceOverRunning) {
    _isVoiceOverRunning = isVoiceOverRunning;
    ApplicationDarwin::InjectEvent(
        kSbEventTypeAccessibilityTextToSpeechSettingsChanged);
  }
}

@end
