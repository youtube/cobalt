// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_CONFIGURATION_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#include <vector>
#import "base/test/scoped_feature_list.h"
#include "components/variations/variations_associated_data.h"

// Enum of relaunch manners. Useful combinations of whether force a relaunch,
// whether kill app gracefully, whether run resets after a relaunch.
typedef NS_ENUM(NSInteger, RelaunchPolicy) {
  // Does not relaunch if app is already running with the same feature list.
  // Kills the app directly. Keeps app state the same as before relaunch.
  NoForceRelaunchAndKeepState,
  // Does not relaunch if app is already running with the same feature list.
  // Kills the app directly. Provides clean test case setups after relaunch.
  NoForceRelaunchAndResetState,
  // Forces a relaunch. Kills the app directly. Keeps app state the same as
  // before relaunch.
  ForceRelaunchByKilling,
  // Forces a relaunch. Backgrounds and then kills the app. Keeps app state same
  // as before relaunch.
  ForceRelaunchByCleanShutdown,
};

// Configuration for launching the app in EGTests.
struct AppLaunchConfiguration {
  // Enabled features.
  std::vector<base::test::FeatureRef> features_enabled;
  // Disabled features.
  std::vector<base::test::FeatureRef> features_disabled;
  // Enabled variations.
  std::vector<variations::VariationID> variations_enabled;
  // Enabled trigger variations.
  std::vector<variations::VariationID> trigger_variations_enabled;
  // Additional arguments to be directly forwarded to the app.
  std::vector<std::string> additional_args;
  // Relaunch policy.
  RelaunchPolicy relaunch_policy = NoForceRelaunchAndResetState;
};

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_CONFIGURATION_H_
