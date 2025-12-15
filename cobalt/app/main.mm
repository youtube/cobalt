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

#import <UIKit/UIKit.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/app/cobalt_switch_defaults.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"

static int g_argc = 0;
static const char** g_argv = nullptr;

@interface CobaltAppSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation CobaltAppSceneDelegate

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

@interface CobaltAppDelegate : UIResponder <UIApplicationDelegate> {
 @private
  std::unique_ptr<content::ContentMainRunner> _mainRunner;
  std::unique_ptr<content::ShellMainDelegate> _mainDelegate;
}
@end

@implementation CobaltAppDelegate

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:
        (UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  UISceneConfiguration* configuration =
      [[UISceneConfiguration alloc] initWithName:nil
                                     sessionRole:connectingSceneSession.role];
  configuration.delegateClass = CobaltAppSceneDelegate.class;
  return configuration;
}

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  const cobalt::CommandLinePreprocessor cobalt_cmd_line(g_argc, g_argv);
  const base::CommandLine::StringVector& processed_argv =
      cobalt_cmd_line.argv();

  // content::ContentMainParams::argv uses regular char** for `argv`, so perform
  // a type conversion here. Note that even though `char_argv` is dependent on
  // the function-scoped `processed_argv`, content::RunContentProcess() copies
  // the data by creating its own base::CommandLine instance.
  std::vector<const char*> char_argv;
  std::ranges::transform(processed_argv, std::back_inserter(char_argv),
                         [](const std::string& arg) { return arg.c_str(); });

  _mainDelegate = std::make_unique<cobalt::CobaltMainDelegate>();
  _mainRunner = content::ContentMainRunner::Create();
  content::ContentMainParams params(_mainDelegate.get());
  params.argc = char_argv.size();
  params.argv = char_argv.data();
  content::RunContentProcess(std::move(params), _mainRunner.get());

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

int main(int argc, const char** argv) {
  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;

  // Even though it is possible to just use `[[NSProcessInfo processInfo]
  // arguments]` later to get the same data, this is simpler since there is no
  // need to convert the data from an NSArray<NSString*>*.
  g_argc = argc;
  g_argv = argv;

  @autoreleasepool {
    return UIApplicationMain(argc, const_cast<char**>(argv), nil,
                             NSStringFromClass([CobaltAppDelegate class]));
  }
}
