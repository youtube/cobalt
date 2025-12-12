// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#import "cobalt/shell/app/ios/shell_application_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/command_line.h"
#include "cobalt/shell/app/shell_main_delegate.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "ui/gfx/geometry/size.h"

static int g_argc = 0;
static const char** g_argv = nullptr;
static std::unique_ptr<content::ContentMainRunner> g_main_runner;
static std::unique_ptr<content::ShellMainDelegate> g_main_delegate;

@interface ShellAppSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation ShellAppSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  CHECK_EQ(1u, content::Shell::windows().size());
  UIWindow* window = content::Shell::windows()[0]->window().Get();

  // The rootViewController must be added after a windowScene is set
  // so stash it in a temp variable and then reattach it. If we don't
  // do this the safe area gets screwed up on orientation changes.
  UIViewController* controller = window.rootViewController;
  window.rootViewController = nil;
  window.windowScene = (UIWindowScene*)scene;
  window.rootViewController = controller;
  [window makeKeyAndVisible];
}

@end

@implementation ShellAppDelegate

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:
        (UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  UISceneConfiguration* configuration =
      [[UISceneConfiguration alloc] initWithName:nil
                                     sessionRole:connectingSceneSession.role];
  configuration.delegateClass = ShellAppSceneDelegate.class;
  return configuration;
}

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  g_main_delegate = std::make_unique<content::ShellMainDelegate>();
  content::ContentMainParams params(g_main_delegate.get());
  params.argc = g_argc;
  params.argv = g_argv;
  g_main_runner = content::ContentMainRunner::Create();
  content::RunContentProcess(std::move(params), g_main_runner.get());
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
}

- (BOOL)application:(UIApplication*)application
    shouldSaveApplicationState:(NSCoder*)coder {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldRestoreApplicationState:(NSCoder*)coder {
  // TODO(crbug.com/710329): Make this value configurable in the settings.
  return YES;
}

@end

int RunShellApplication(int argc, const char** argv) {
  g_argc = argc;
  g_argv = argv;
  @autoreleasepool {
    return UIApplicationMain(argc, const_cast<char**>(argv), nil,
                             NSStringFromClass([ShellAppDelegate class]));
  }
}
