// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_service_impl.h"

namespace borealis {

BorealisServiceImpl::BorealisServiceImpl(Profile* profile)
    : profile_(profile),
      app_launcher_(profile_),
      app_uninstaller_(profile_),
      context_manager_(profile),
      disk_manager_dispatcher_(),
      features_(profile_),
      installer_(profile_),
      launch_options_(profile_),
      shutdown_monitor_(profile_),
      window_manager_(profile_) {}

BorealisServiceImpl::~BorealisServiceImpl() = default;

BorealisAppLauncher& BorealisServiceImpl::AppLauncher() {
  return app_launcher_;
}

BorealisAppUninstaller& BorealisServiceImpl::AppUninstaller() {
  return app_uninstaller_;
}

BorealisContextManager& BorealisServiceImpl::ContextManager() {
  return context_manager_;
}

BorealisDiskManagerDispatcher& BorealisServiceImpl::DiskManagerDispatcher() {
  return disk_manager_dispatcher_;
}

BorealisFeatures& BorealisServiceImpl::Features() {
  return features_;
}

BorealisInstaller& BorealisServiceImpl::Installer() {
  return installer_;
}

BorealisLaunchOptions& BorealisServiceImpl::LaunchOptions() {
  return launch_options_;
}

BorealisShutdownMonitor& BorealisServiceImpl::ShutdownMonitor() {
  return shutdown_monitor_;
}

BorealisWindowManager& BorealisServiceImpl::WindowManager() {
  return window_manager_;
}

}  // namespace borealis
