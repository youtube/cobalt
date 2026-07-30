// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_

class PrefService;

namespace personal_context {
class PersonalContextEnablementService;
}  // namespace personal_context

namespace autofill {

// An AtMemory-related action that a user may take (directly or indirectly).
enum class AtMemoryAction {
  // Trigger main AtMemory component using the keyboard invocation.
  kTriggerSearchUI,
  // Show any AtMemory related settings in the Enhanced Autofill section of
  // the settings. This evaluates to true regardless of the Personal Context
  // toggle state.
  // This does not imply that the settings are functional.
  // E.g. for the AtMemoryShortcut customization there is a separate
  // `AtMemoryAction`: `kAllowCustomizeAtMemoryShortcut` to verify if it's
  // functional.
  kShowAtMemoryInSettings,
  // Allow the user to customize the AtMemory shortcut.
  // This unlocks the ability to reconfigure the shortcut in Enhanced
  // Autofill section of the Settings.
  kAllowCustomizeAtMemoryShortcut,
};

// Returns whether all permission-related requirements are met for `action`.
//
// Checks that AtMemory feature flags are enabled, AtMemory eligibility
// criteria are met and PersonalContext settings toggle is on if required by
// the action.
[[nodiscard]] bool MayPerformAtMemoryAction(
    AtMemoryAction action,
    personal_context::PersonalContextEnablementService*
        personal_context_service,
    const PrefService* pref_service);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_ENABLEMENT_UTILS_H_
