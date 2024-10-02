// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for NtpCustomBackgroundService.
class NtpCustomBackgroundServiceObserver : public base::CheckedObserver {
 public:
  // Called when the custom background image is updated.
  virtual void OnCustomBackgroundImageUpdated() = 0;

  // Called when the NtpCustomBackgroundService is shutting down. Observers that
  // might outlive the service should use this to unregister themselves, and
  // clear out any pointers to the service they might hold.
  virtual void OnNtpCustomBackgroundServiceShuttingDown() {}
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
