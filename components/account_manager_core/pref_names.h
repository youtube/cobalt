// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_PREF_NAMES_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_PREF_NAMES_H_

#include "base/component_export.h"

namespace account_manager {
namespace prefs {

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
extern const char kSecondaryGoogleAccountSigninAllowed[];

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
extern const char kAccountAppsAvailability[];

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
extern const char kIsAvailableInArcKey[];

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
extern const char kSecondaryGoogleAccountUsage[];

}  // namespace prefs
}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_PREF_NAMES_H_
