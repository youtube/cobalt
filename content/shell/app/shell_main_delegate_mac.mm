// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_main_delegate_mac.h"

#include <unistd.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/browser/shell_application_mac.h"
#include "content/shell/common/shell_switches.h"

namespace content {

void EnsureCorrectResolutionSettings() {
  // Exit early if this isn't a browser process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    return;
  }

  NSString* const kHighResolutionCapable = @"NSHighResolutionCapable";
  base::FilePath info_plist = GetInfoPlistPath();
  NSMutableDictionary* info_dict = [[NSMutableDictionary alloc]
      initWithContentsOfFile:base::apple::FilePathToNSString(info_plist)];

  bool running_web_tests = switches::IsRunWebTestsSwitchPresent();
  NSNumber* high_resolution_capable_from_info_dict =
      [info_dict objectForKey:kHighResolutionCapable];
  bool not_high_resolution_capable =
      high_resolution_capable_from_info_dict &&
      !high_resolution_capable_from_info_dict.boolValue;
  if (running_web_tests == not_high_resolution_capable) {
    return;
  }

  // We need to update our Info.plist before we can continue.
  [info_dict setObject:@(!running_web_tests) forKey:kHighResolutionCapable];
  CHECK([info_dict writeToFile:base::apple::FilePathToNSString(info_plist)
                    atomically:YES]);

  const base::CommandLine::StringVector& original_argv =
      base::CommandLine::ForCurrentProcess()->argv();
  char** argv = new char*[original_argv.size() + 1];
  for (unsigned i = 0; i < original_argv.size(); ++i) {
    argv[i] = const_cast<char*>(original_argv.at(i).c_str());
  }
  argv[original_argv.size()] = nullptr;

  CHECK(execvp(argv[0], argv));
}

void OverrideBundleID() {
  NSBundle* bundle = base::apple::OuterBundle();
  base::apple::SetBaseBundleID(
      base::SysNSStringToUTF8([bundle bundleIdentifier]).c_str());
}

void RegisterShellCrApp() {
  // Force the NSApplication subclass to be used.
  [ShellCrApplication sharedApplication];

  // If there was an invocation to NSApp prior to this method, then the NSApp
  // will not be a ShellCrApplication, but will instead be an NSApplication.
  // This is undesirable and we must enforce that this doesn't happen.
  CHECK([NSApp isKindOfClass:[ShellCrApplication class]]);
}

}  // namespace content
