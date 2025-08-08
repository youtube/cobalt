// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_path.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/send_tab_to_self/features.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/content_notification/model/content_notification_settings_action.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/model/push_notification_delegate.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {
// The time range's expected min and max values for custom histograms.
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMin =
    base::Milliseconds(1);
constexpr base::TimeDelta kTimeRangeIncomingNotificationHistogramMax =
    base::Seconds(30);
// Number of buckets for the time range histograms.
constexpr int kTimeRangeHistogramBucketCount = 30;

// The histogram used to record a push notification's current lifecycle state on
// the device.
const char kLifecycleEventsHistogram[] = "IOS.PushNotification.LifecyleEvents";

// This enum is used to represent a point along the push notification's
// lifecycle.
enum class PushNotificationLifecycleEvent {
  kNotificationReception,
  kNotificationForegroundPresentation,
  kNotificationInteraction,
  kMaxValue = kNotificationInteraction
};

// This function creates a dictionary that maps signed-in user's GAIA IDs to a
// map of each user's preferences for each push notification enabled feature.
GaiaIdToPushNotificationPreferenceMap*
GaiaIdToPushNotificationPreferenceMapFromCache() {
  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();
  ProfileAttributesStorageIOS* storage = manager->GetProfileAttributesStorage();
  NSMutableDictionary* account_preference_map =
      [[NSMutableDictionary alloc] init];

  // TODO(crbug.com/383315797): Migrate prefs to ProfileAttributesStorageIOS.
  for (ProfileIOS* profile : manager->GetLoadedProfiles()) {
    ProfileAttributesIOS attr =
        storage->GetAttributesForProfileWithName(profile->GetProfileName());
    if (attr.GetGaiaId().empty()) {
      continue;
    }

    PrefService* pref_service = profile->GetPrefs();
    NSMutableDictionary<NSString*, NSNumber*>* preference_map =
        [[NSMutableDictionary alloc] init];
    const base::Value::Dict& permissions =
        pref_service->GetDict(prefs::kFeaturePushNotificationPermissions);

    for (const auto pair : permissions) {
      preference_map[base::SysUTF8ToNSString(pair.first)] =
          [NSNumber numberWithBool:pair.second.GetBool()];
    }

    account_preference_map[base::SysUTF8ToNSString(attr.GetGaiaId())] =
        preference_map;
  }

  return account_preference_map;
}

// Call ContentNotificationService::SendNAUForConfiguration() after fetching
// the notification settings if `weak_profile` is still valid.
void SendNAUFConfigurationForProfileWithSettings(
    base::WeakPtr<ProfileIOS> weak_profile,
    UNNotificationSettings* settings) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  UNAuthorizationStatus previousAuthStatus =
      [PushNotificationUtil getSavedPermissionSettings];
  ContentNotificationNAUConfiguration* config =
      [[ContentNotificationNAUConfiguration alloc] init];
  ContentNotificationSettingsAction* settingsAction =
      [[ContentNotificationSettingsAction alloc] init];
  settingsAction.previousAuthorizationStatus = previousAuthStatus;
  settingsAction.currentAuthorizationStatus = settings.authorizationStatus;
  config.settingsAction = settingsAction;
  ContentNotificationServiceFactory::GetForProfile(profile)
      ->SendNAUForConfiguration(config);
}

}  // anonymous namespace

@interface PushNotificationDelegate () <AppStateObserver,
                                        ProfileStateObserver,
                                        SceneStateObserver>
@end

@implementation PushNotificationDelegate {
  __weak AppState* _appState;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _appState = appState;
    [_appState addObserver:self];
  }
  return self;
}

#pragma mark - UNUserNotificationCenterDelegate -

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationInteraction];
  // This method is invoked by iOS to process the user's response to a delivered
  // notification.
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->HandleNotificationInteraction(response);
  if (completionHandler) {
    completionHandler();
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationForegroundPresentation];
  // This method is invoked by iOS to process a notification that arrived while
  // the app was running in the foreground.
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->HandleNotificationReception(
      notification.request.content.userInfo);

  if (completionHandler) {
    // If the app is foregrounded, Send Tab push notifications should not be
    // displayed.
    if ([notification.request.content.userInfo[kPushNotificationClientIdKey]
            intValue] == static_cast<int>(PushNotificationClientId::kSendTab) &&
        [self isSceneLevelForegroundActive]) {
      completionHandler(UNNotificationPresentationOptionNone);
    }
    completionHandler(UNNotificationPresentationOptionBanner);
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::NOTIFICATION);
}

#pragma mark - PushNotificationDelegate

- (UIBackgroundFetchResult)applicationWillProcessIncomingRemoteNotification:
    (NSDictionary*)userInfo {
  [self recordLifeCycleEvent:PushNotificationLifecycleEvent::
                                 kNotificationReception];

  double incomingNotificationTime =
      base::Time::Now().InSecondsFSinceUnixEpoch();
  auto* clientManager = GetApplicationContext()
                            ->GetPushNotificationService()
                            ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  UIBackgroundFetchResult result =
      clientManager->HandleNotificationReception(userInfo);

  double processingTime =
      base::Time::Now().InSecondsFSinceUnixEpoch() - incomingNotificationTime;
  UmaHistogramCustomTimes(
      "IOS.PushNotification.IncomingNotificationProcessingTime",
      base::Milliseconds(processingTime),
      kTimeRangeIncomingNotificationHistogramMin,
      kTimeRangeIncomingNotificationHistogramMax,
      kTimeRangeHistogramBucketCount);
  return result;
}

- (void)applicationDidRegisterWithAPNS:(NSData*)deviceToken
                               profile:(ProfileIOS*)profile {
  GaiaIdToPushNotificationPreferenceMap* accountPreferenceMap =
      GaiaIdToPushNotificationPreferenceMapFromCache();

  // Return early if no accounts are signed into Chrome.
  if (!accountPreferenceMap.count) {
    return;
  }

  PushNotificationService* notificationService =
      GetApplicationContext()->GetPushNotificationService();

  // Registers Chrome's PushNotificationClients' Actionable Notifications with
  // iOS.
  notificationService->GetPushNotificationClientManager()
      ->RegisterActionableNotifications();

  PushNotificationConfiguration* config =
      [[PushNotificationConfiguration alloc] init];

  config.accountIDs = accountPreferenceMap.allKeys;
  config.preferenceMap = accountPreferenceMap;
  config.deviceToken = deviceToken;
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();

  if (profile) {
    config.shouldRegisterContentNotification =
        [self isContentNotificationAvailable:profile];
    if (config.shouldRegisterContentNotification) {
      AuthenticationService* authService =
          AuthenticationServiceFactory::GetForProfile(profile);
      id<SystemIdentity> identity =
          authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      config.primaryAccount = identity;
      // Send an initial NAU to share the OS auth status and channel status with
      // the server. Send an NAU on every foreground to report the OS Auth
      // Settings.
      [self sendSettingsChangeNAUForProfile:profile];
    }
  }

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<ProfileIOS> weakProfile =
      profile ? profile->AsWeakPtr() : base::WeakPtr<ProfileIOS>{};

  notificationService->RegisterDevice(config, ^(NSError* error) {
    [weakSelf deviceRegistrationForProfile:weakProfile withError:error];
  });
}

- (void)deviceRegistrationForProfile:(base::WeakPtr<ProfileIOS>)weakProfile
                           withError:(NSError*)error {
  base::UmaHistogramBoolean("IOS.PushNotification.ChimeDeviceRegistration",
                            !error);
  if (!error) {
    if (ProfileIOS* profile = weakProfile.get()) {
      if (base::FeatureList::IsEnabled(
              send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
        [self setUpAndEnableSendTabNotificationsWithProfile:profile];
      }
    }
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
  [self sceneState:sceneState
      transitionedToActivationLevel:sceneState.activationLevel];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage < ProfileInitStage::kFinal) {
    return;
  }

  for (SceneState* sceneState in profileState.connectedScenes) {
    if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
      continue;
    }

    [self appDidEnterForeground:sceneState];
  }

  [profileState removeObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level < SceneActivationLevelForegroundActive) {
    return;
  }

  if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    [sceneState.profileState addObserver:self];
    return;
  }

  [self appDidEnterForeground:sceneState];
}

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  [profileState addObserver:self];
}

#pragma mark - Private

// Notifies the client manager that the scene is "foreground active".
- (void)appDidEnterForeground:(SceneState*)sceneState {
  PushNotificationClientManager* clientManager =
      GetApplicationContext()
          ->GetPushNotificationService()
          ->GetPushNotificationClientManager();
  DCHECK(clientManager);
  clientManager->OnSceneActiveForegroundBrowserReady();
  ProfileIOS* profile = sceneState.browserProviderInterface.mainBrowserProvider
                            .browser->GetProfile();
  if (IsContentNotificationEnabled(profile)) {
    ContentNotificationService* contentNotificationService =
        ContentNotificationServiceFactory::GetForProfile(profile);
    int maxNauSentPerSession = base::GetFieldTrialParamByFeatureAsInt(
        kContentNotificationDeliveredNAU, kDeliveredNAUMaxPerSession,
        kDeliveredNAUMaxSendsPerSession);
    // Check if there are notifications received in the background to send the
    // respective NAUs.
    NSUserDefaults* defaults = app_group::GetGroupUserDefaults();
    if ([defaults objectForKey:kContentNotificationContentArrayKey] != nil) {
      NSMutableArray* contentArray = [[defaults
          objectForKey:kContentNotificationContentArrayKey] mutableCopy];
      // Report in 5 item increments.
      NSMutableArray* uploadedItems = [NSMutableArray array];
      for (NSData* item in contentArray) {
        ContentNotificationNAUConfiguration* config =
            [[ContentNotificationNAUConfiguration alloc] init];
        config.actionType = NAUActionTypeDisplayed;
        UNNotificationContent* content = [NSKeyedUnarchiver
            unarchivedObjectOfClass:UNMutableNotificationContent.class
                           fromData:item
                              error:nil];
        config.content = content;
        contentNotificationService->SendNAUForConfiguration(config);
        [uploadedItems addObject:item];
        base::UmaHistogramEnumeration(
            kContentNotificationActionHistogramName,
            NotificationActionType::kNotificationActionTypeDisplayed);
        if ((int)uploadedItems.count == maxNauSentPerSession) {
          break;
        }
      }
      [contentArray removeObjectsInArray:uploadedItems];
      if (contentArray.count > 0) {
        [defaults setObject:contentArray
                     forKey:kContentNotificationContentArrayKey];
      } else {
        [defaults setObject:nil forKey:kContentNotificationContentArrayKey];
      }
    }
    // Send an NAU on every foreground to report the OS Auth Settings.
    [self sendSettingsChangeNAUForProfile:profile];
  }
  [PushNotificationUtil updateAuthorizationStatusPref];

  // For Reactivation Notifications, ask for provisional auth right away.
  if (IsFirstRunRecent(base::Days(28)) &&
      IsIOSReactivationNotificationsEnabled()) {
    UNAuthorizationStatus auth_status =
        [PushNotificationUtil getSavedPermissionSettings];
    if (auth_status == UNAuthorizationStatusNotDetermined) {
      [PushNotificationUtil enableProvisionalPushNotificationPermission:nil];
    }
  }
}

- (void)sendSettingsChangeNAUForProfile:(ProfileIOS*)profile {
  [PushNotificationUtil
      getPermissionSettings:base::CallbackToBlock(base::BindOnce(
                                &SendNAUFConfigurationForProfileWithSettings,
                                profile->AsWeakPtr()))];
}

- (void)recordLifeCycleEvent:(PushNotificationLifecycleEvent)event {
  base::UmaHistogramEnumeration(kLifecycleEventsHistogram, event);
}

- (BOOL)isContentNotificationAvailable:(ProfileIOS*)profile {
  return IsContentNotificationEnabled(profile) ||
         IsContentNotificationRegistered(profile);
}

// Returns YES if there is a foreground active scene for any profile.
- (BOOL)isSceneLevelForegroundActive {
  for (SceneState* sceneState in _appState.connectedScenes) {
    if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
      continue;
    }

    if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
      continue;
    }

    return YES;
  }

  return NO;
}

// If user has not previously disabled Send Tab notifications, either 1) If user
// has authorized full notification permissions, enables Send Tab notifications
// OR 2) enrolls user in provisional notifications for Send Tab notification
// type.
- (void)setUpAndEnableSendTabNotificationsWithProfile:(ProfileIOS*)profile {
  // Refresh the local device info now that the client has a Chime
  // Representative Target ID.
  syncer::DeviceInfoSyncService* deviceInfoSyncService =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  deviceInfoSyncService->RefreshLocalDeviceInfo();

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  NSString* gaiaID =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin).gaiaID;

  // Early return if 1) the user has previously disabled Send Tab push
  // notifications, because in that case we don't want to automatically enable
  // the notification type or 2) if Send Tab notifications are already enabled.
  if (profile->GetPrefs()->GetBoolean(
          prefs::kSendTabNotificationsPreviouslyDisabled) ||
      push_notification_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kSendTab,
              base::SysNSStringToUTF8(gaiaID))) {
    return;
  }

  if ([PushNotificationUtil getSavedPermissionSettings] ==
      UNAuthorizationStatusAuthorized) {
    GetApplicationContext()->GetPushNotificationService()->SetPreference(
        gaiaID, PushNotificationClientId::kSendTab, true);
  } else {
    [ProvisionalPushNotificationUtil
        enrollUserToProvisionalNotificationsForClientIds:
            {PushNotificationClientId::kSendTab}
                             clientEnabledForProvisional:YES
                                         withAuthService:authService
                                   deviceInfoSyncService:deviceInfoSyncService];
  }
}

@end
