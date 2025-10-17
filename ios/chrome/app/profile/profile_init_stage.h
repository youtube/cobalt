// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_

// Profile initialisation stages. The app will go sequentially in-order through
// each stage each time a new profile is added.
enum class ProfileInitStage {
  // Initial stage, nothing initialized yet.
  kStart,

  // Perform all asynch operation to load profile's preferences from disk.
  kLoadProfile,

  // Migrate the session storage for the profile.
  // TODO(crbug.com/40945317): remove when migrating legacy session storage
  // is no longer supported (i.e. all users have migrated).
  kMigrateStorage,

  // Delete all data for previously discarded sessions. If no sessions was
  // recently discarded, this will transition immediately to the next stage.
  kPurgeDiscardedSessionsData,

  // Profile preferences have been loaded and the ProfileIOS object and all
  // KeyedServices can be used. The app will automatically transition to the
  // next stage.
  kProfileLoaded,

  // The application is loading any elements needed for UI for this profile
  // (e.g. Session data, ...)
  kPrepareUI,

  // The application is ready to present UI for the profile, it will transition
  // to the next stage. This can be used to start background tasks to update UI.
  kUIReady,

  // All the stage between kUIReady and kNormalUI represent blocking screens
  // that the user must go through before proceeding to the next stage. If the
  // conditions are already handled, the transition will be instantanous.
  //
  // It is possible to add new stage between kUIReady and kNormalUI to add new
  // blocking stage if a feature requires it.

  // This presents the first run experience. Only presented for new profile
  // (maybe first profile?)
  kFirstRun,

  // This presents the search engine selection screen. It is presented for each
  // profile and if the user did not select a default search engine yet.
  kChoiceScreen,

  // The application is presenting the regular chrome UI for this profile, it
  // will automatically transition to the next stage. This can be used to detect
  // that users can now start interacting with the UI.
  kNormalUI,

  // Final stage, no transition until the profile is shut down.
  kFinal,
};

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
