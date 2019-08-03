// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#import <UIKit/UIKit.h>

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"

// Springboard will kill any iOS app that fails to check in after launch within
// a given time. These two classes prevent this from happening.

// MainHook saves the chrome main() and calls UIApplicationMain(),
// providing an application delegate class: ChromeUnitTestDelegate. The delegate
// listens for UIApplicationDidFinishLaunchingNotification. When the
// notification is received, it fires main() again to have the real work done.

// Example usage:
// int main(int argc, char** argv) {
//   MainHook hook(main, argc, argv);
//   // Testing code goes here. There should be no code above MainHook. If
//   // there is, it will be run twice.
// }

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

static MainHook::MainType g_main_func = NULL;
static int g_argc;
static char** g_argv;

@interface UIApplication (Testing)
- (void) _terminateWithStatus:(int)status;
@end

@interface ChromeUnitTestDelegate : NSObject {
@private
  scoped_nsobject<UIWindow> window_;
}
- (void)runTests;
@end

@implementation ChromeUnitTestDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

  CGRect bounds = [[UIScreen mainScreen] bounds];

  // Yes, this is leaked, it's just to make what's running visible.
  window_.reset([[UIWindow alloc] initWithFrame:bounds]);
  [window_ makeKeyAndVisible];

  // Add a label with the app name.
  UILabel* label = [[[UILabel alloc] initWithFrame:bounds] autorelease];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = UITextAlignmentCenter;
  [window_ addSubview:label];

  // Queue up the test run.
  [self performSelector:@selector(runTests)
             withObject:nil
             afterDelay:0.1];
  return YES;
}

- (void)runTests {
  int exitStatus = g_main_func(g_argc, g_argv);

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(ios): crbug.com/137010 Figure out how much time is actually needed,
  // and sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];
  window_.reset();

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

#pragma mark -

MainHook::MainHook(MainType main_func, int argc, char* argv[]) {
  static bool ran_hook = false;
  if (!ran_hook) {
    ran_hook = true;

    g_main_func = main_func;
    g_argc = argc;
    g_argv = argv;

    base::mac::ScopedNSAutoreleasePool pool;
    int exit_status = UIApplicationMain(argc, argv, nil,
                                        @"ChromeUnitTestDelegate");
    exit(exit_status);
  }
}
