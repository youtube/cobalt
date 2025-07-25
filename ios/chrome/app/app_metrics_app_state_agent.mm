// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/metrics/histogram_macros.h"
#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

@interface AppMetricsAppStateAgent () <SceneStateObserver>

// Observed app state.
@property(nonatomic, weak) AppState* appState;

// This flag is set when the first scene has activated since the startup, and
// never reset during the app's lifetime.
@property(nonatomic, assign) BOOL firstSceneHasActivated;

// This flag is set when the first scene has connected since the startup, and
// never reset during the app's lifetime.
@property(nonatomic, assign) BOOL firstSceneHasConnected;

@end

@implementation AppMetricsAppStateAgent

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (previousInitStage == InitStageSafeMode) {
    // Log session start if the app is already foreground
    if (self.appState.foregroundScenes.count > 0) {
      [self handleSessionStart];
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!self.firstSceneHasConnected) {
    self.appState.startupInformation.firstSceneConnectionTime =
        base::TimeTicks::Now();
    self.firstSceneHasConnected = YES;
    if (self.appState.initStage > InitStageSafeMode)
      [MetricsMediator createStartupTrackingTask];
  }

  if (self.appState.initStage <= InitStageSafeMode) {
    return;
  }

  if (level >= SceneActivationLevelForegroundInactive &&
      self.appState.lastTimeInForeground.is_null()) {
    [self handleSessionStart];
  } else if (level <= SceneActivationLevelBackground) {
    // Do not consider the app as brackgrounded when there are still scenes on
    // the foreground.
    if (self.appState.foregroundScenes.count > 0) {
      return;
    }
    if (self.appState.lastTimeInForeground.is_null()) {
      // This method will be called multiple times, once per scene, if multiple
      // scenes go background simulatneously (for example, if two windows were
      // in split screen and the user swiped to go home). Only log the session
      // duration once. This also makes sure that the first scene that ramps up
      // to foreground doesn't end the session.
      return;
    }

    [self handleSessionEnd];
    DCHECK(self.appState.lastTimeInForeground.is_null());
  }

  if (level >= SceneActivationLevelForegroundActive) {
    if (!self.firstSceneHasActivated) {
      self.firstSceneHasActivated = YES;
      [MetricsMediator logStartupDuration:self.appState.startupInformation];
      if (ios::provider::IsPrimesSupported()) {
        ios::provider::PrimesAppReady();
      }
    }
  }
}

#pragma mark - private

- (void)handleSessionStart {
  self.appState.lastTimeInForeground = base::TimeTicks::Now();

  IOSProfileSessionDurationsService* psdService = [self psdService];
  if (psdService) {
    psdService->OnSessionStarted(self.appState.lastTimeInForeground);
  }
}

- (void)handleSessionEnd {
  DCHECK(!self.appState.lastTimeInForeground.is_null());

  base::TimeDelta duration =
      base::TimeTicks::Now() - self.appState.lastTimeInForeground;

  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", duration);
  UMA_HISTOGRAM_CUSTOM_TIMES("Session.TotalDurationMax1Day", duration,
                             base::Milliseconds(1), base::Hours(24), 50);

  IOSProfileSessionDurationsService* psdService = [self psdService];
  if (psdService) {
    psdService->OnSessionEnded(duration);
  }

  self.appState.lastTimeInForeground = base::TimeTicks();
}

- (IOSProfileSessionDurationsService*)psdService {
  if (!self.appState.mainBrowserState) {
    return nil;
  }

  return IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
      self.appState.mainBrowserState);
}

@end
