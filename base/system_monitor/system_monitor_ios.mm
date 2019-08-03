// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#import <UIKit/UIKit.h>

namespace base {

void SystemMonitor::PlatformInit() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  id foreground =
      [nc addObserverForName:UIApplicationWillEnterForegroundNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      ProcessPowerMessage(RESUME_EVENT);
                  }];
  id background =
      [nc addObserverForName:UIApplicationDidEnterBackgroundNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      ProcessPowerMessage(SUSPEND_EVENT);
                  }];
  notification_observers_.push_back(foreground);
  notification_observers_.push_back(background);
}

void SystemMonitor::PlatformDestroy() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  for (std::vector<id>::iterator it = notification_observers_.begin();
       it != notification_observers_.end(); ++it) {
    [nc removeObserver:*it];
  }
  notification_observers_.clear();
}

}  // namespace base
