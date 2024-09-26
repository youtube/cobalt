// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/application_breadcrumbs_logger.h"

#import <string>

#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kBreadcrumbOrientation[] = "Orientation";

ApplicationBreadcrumbsLogger::ApplicationBreadcrumbsLogger(
    const base::FilePath& storage_dir)
    : breadcrumbs::ApplicationBreadcrumbsLogger(
          storage_dir,
          base::BindRepeating(&IOSChromeMetricsServiceAccessor::
                                  IsMetricsAndCrashReportingEnabled)) {
  orientation_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIDeviceOrientationDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (UIDevice.currentDevice.orientation == last_orientation_)
                  return;
                last_orientation_ = UIDevice.currentDevice.orientation;

                std::string event(kBreadcrumbOrientation);
                switch (UIDevice.currentDevice.orientation) {
                  case UIDeviceOrientationUnknown:
                    event += " #unknown";
                    break;
                  case UIDeviceOrientationPortrait:
                    event += " #portrait";
                    break;
                  case UIDeviceOrientationPortraitUpsideDown:
                    event += " #portrait-upside-down";
                    break;
                  case UIDeviceOrientationLandscapeLeft:
                    event += " #landscape-left";
                    break;
                  case UIDeviceOrientationLandscapeRight:
                    event += " #landscape-right";
                    break;
                  case UIDeviceOrientationFaceUp:
                    event += " #face-up";
                    break;
                  case UIDeviceOrientationFaceDown:
                    event += " #face-down";
                    break;
                }
                breadcrumbs::BreadcrumbManager::GetInstance().AddEvent(event);
              }];
}

ApplicationBreadcrumbsLogger::~ApplicationBreadcrumbsLogger() {
  [NSNotificationCenter.defaultCenter removeObserver:orientation_observer_];
}
