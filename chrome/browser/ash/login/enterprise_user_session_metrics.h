// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_

#include "base/time/time.h"
#include "components/user_manager/user_type.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace enterprise_user_session_metrics {

// Register local state preferences.
void RegisterPrefs(PrefRegistrySimple* registry);

// Stores session length for regular user, public session user for enrolled
// device to be reported on the next run. It stores the duration in a local
// state pref instead of sending it to metrics code directly because it is
// called on shutdown path and metrics are likely to be lost. The stored value
// would be reported on the next run.
void StoreSessionLength(PrefService& local_state,
                        user_manager::UserType session_type,
                        const base::TimeDelta& session_length);

// Records the stored session length and clears it.
void RecordStoredSessionLength(PrefService& local_state);

}  // namespace enterprise_user_session_metrics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_
