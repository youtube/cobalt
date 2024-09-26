// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_STORE_UTILS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_STORE_UTILS_H_

class ChromeBrowserState;

// Query the password stores and reports multiple metrics. The actual reporting
// is delayed by 30 seconds, to ensure it doesn't happen during the "hot phase"
// of Chrome startup.
void DelayReportingPasswordStoreMetrics(ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_STORE_UTILS_H_
