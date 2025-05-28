// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             "DeferredSyncStartupCustomDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableBatchUploadFromSettings,
             "EnableBatchUploadFromSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnoPhase2FollowUp,
             "UnoPhase2FollowUp",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletCredentialData,
             "SyncAutofillWalletCredentialData",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSyncChromeOSAppsToggleSharing,
             "SyncChromeOSAppsToggleSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated,
             "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             "SyncEnableContactInfoDataTypeForCustomPassphraseUsers",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePasswordsAccountStorageForSyncingUsers,
             "EnablePasswordsAccountStorageForSyncingUsers",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             "SyncEnableContactInfoDataTypeForDasherUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsSaveNudgeDelay,
             "TabGroupsSaveNudgeDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountSearchEngines,
             "SeparateLocalAndAccountSearchEngines",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
             "ReplaceSyncPromosWithSignInPromos",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableBookmarksInTransportMode,
             "SyncEnableBookmarksInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_IOS)
);

BASE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting,
             "EnableBookmarksSelectedTypeOnSigninForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             "ReadingListEnableSyncTransportModeUponSignIn",
             base::FEATURE_DISABLED_BY_DEFAULT
);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncSharedTabGroupDataInTransportMode,
             "SyncSharedTabGroupDataInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableWalletMetadataInTransportMode,
             "SyncEnableWalletMetadataInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableWalletOfferInTransportMode,
             "SyncEnableWalletOfferInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions,
             "SyncPasswordCleanUpAccidentalBatchDeletions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateAccountPrefs,
             "MigrateAccountPrefs",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSeparateLocalAndAccountThemes,
             "SeparateLocalAndAccountThemes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThemesBatchUpload,
             "ThemesBatchUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             "SyncIncreaseNudgeDelayForSingleClient",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMoveThemePrefsToSpecifics,
             "MoveThemePrefsToSpecifics",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebApkBackupAndRestoreBackend,
             "WebApkBackupAndRestoreBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncEnableExtensionsInTransportMode,
             "SyncEnableExtensionsInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncEnablePasswordsSyncErrorMessageAlternative,
             "SyncEnablePasswordsSyncErrorMessageAlternative",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace syncer
