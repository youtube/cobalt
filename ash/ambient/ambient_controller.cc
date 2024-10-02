// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/ambient_video_ui_launcher.h"
#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"
#include "ash/ambient/model/ambient_topic_queue_slideshow_delegate.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"
#include "ash/ambient/ui/ambient_animation_progress_tracker.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ash_features.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_metrics.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_status.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "cc/paint/skottie_wrapper.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)

namespace ash {

namespace {

// Used by wake lock APIs.
constexpr char kWakeLockReason[] = "AmbientMode";

// Time taken from releasing wake lock to turning off display.
// NOTE: This value was found experimentally and is temporarily here until the
// source of the delay is resolved.
// TODO(b/278939395): Find the code that causes this delay.
constexpr base::TimeDelta kReleaseWakeLockDelay = base::Seconds(38);

// kAmbientModeRunningDurationMinutes with value 0 means "forever".
constexpr int kDurationForever = 0;

std::unique_ptr<AmbientBackendController> CreateAmbientBackendController() {
#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
  return std::make_unique<AmbientBackendControllerImpl>();
#else
  return std::make_unique<FakeAmbientBackendControllerImpl>();
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
}

// Returns the name of the ambient widget.
std::string GetWidgetName() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock))
    return "LockScreenAmbientModeContainer";
  return "InSessionAmbientModeContainer";
}

// Returns true if the device is currently connected to a charger.
bool IsChargerConnected() {
  DCHECK(PowerStatus::IsInitialized());
  auto* power_status = PowerStatus::Get();
  if (power_status->IsBatteryPresent()) {
    // If battery is full or battery is charging, that implies power is
    // connected. Also return true if a power source is connected and
    // battery is not discharging.
    return power_status->IsBatteryCharging() ||
           (power_status->IsLinePowerConnected() &&
            power_status->GetBatteryPercent() > 95.f);
  } else {
    // Chromeboxes have no battery.
    return power_status->IsLinePowerConnected();
  }
}

bool IsUiHidden(AmbientUiVisibility visibility) {
  return visibility == AmbientUiVisibility::kHidden;
}

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

PrefService* GetSigninPrefService() {
  return Shell::Get()->session_controller()->GetSigninScreenPrefService();
}

PrefService* GetActivePrefService() {
  if (GetPrimaryUserPrefService()) {
    return GetPrimaryUserPrefService();
  }
  if (ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return GetSigninPrefService();
  }
  return nullptr;
}

bool IsUserAmbientModeEnabled() {
  if (!AmbientClient::Get()->IsAmbientModeAllowed()) {
    return false;
  }

  auto* pref_service = GetPrimaryUserPrefService();
  return pref_service &&
         pref_service->GetBoolean(ambient::prefs::kAmbientModeEnabled);
}

bool IsAmbientModeManagedScreensaverEnabled() {
  PrefService* pref_service = GetActivePrefService();

  return ash::features::IsAmbientModeManagedScreensaverEnabled() &&
         pref_service &&
         pref_service->GetBoolean(
             ambient::prefs::kAmbientModeManagedScreensaverEnabled);
}

bool IsAmbientModeEnabled() {
  return IsUserAmbientModeEnabled() || IsAmbientModeManagedScreensaverEnabled();
}

// Get the cache root path for ambient mode.
base::FilePath GetCacheRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

class AmbientWidgetDelegate : public views::WidgetDelegate {
 public:
  AmbientWidgetDelegate() {
    SetCanMaximize(true);
    SetOwnedByWidget(true);
  }
};

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(ash::ambient::prefs::kAmbientBackdropClientId,
                               std::string());

  // Do not sync across devices to allow different usages for different
  // devices.
  registry->RegisterBooleanPref(ash::ambient::prefs::kAmbientModeEnabled,
                                false);

  // Used to upload usage metrics. Derived from |AmbientSettings| when
  // settings are successfully saved by the user. This pref is not displayed
  // to the user.
  registry->RegisterIntegerPref(
      ash::ambient::prefs::kAmbientModePhotoSourcePref,
      static_cast<int>(ash::ambient::AmbientModePhotoSource::kUnset));

  // Used to control the number of seconds of inactivity on lock screen before
  // showing Ambient mode. This pref is not displayed to the user. Registered
  // as integer rather than TimeDelta to work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      kLockScreenInactivityTimeout.InSeconds());

  // Used to control the number of seconds to lock the session after starting
  // Ambient mode. This pref is not displayed to the user. Registered as
  // integer rather than TimeDelta to work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      kLockScreenBackgroundTimeout.InSeconds());

  // Used to control the photo refresh interval in Ambient mode. This pref is
  // not displayed to the user. Registered as integer rather than TimeDelta to
  // work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
      kPhotoRefreshInterval.InSeconds());

  // |ambient::prefs::kAmbientTheme| is for legacy purposes only. It is being
  // migrated to |ambient::prefs::kAmbientUiSettings|, which is the newer
  // version of these settings.
  registry->RegisterIntegerPref(ambient::prefs::kAmbientTheme,
                                static_cast<int>(kDefaultAmbientTheme));
  registry->RegisterDictionaryPref(ambient::prefs::kAmbientUiSettings,
                                   base::Value::Dict());

  registry->RegisterDoublePref(
      ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
      kAnimationPlaybackSpeed);

  registry->RegisterBooleanPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverEnabled, false);

  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
      kManagedScreensaverInactivityTimeout.InSeconds());

  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
      kManagedScreensaverImageRefreshInterval.InSeconds());

  if (ash::features::IsScreenSaverDurationEnabled()) {
    registry->RegisterIntegerPref(
        ambient::prefs::kAmbientModeRunningDurationMinutes,
        kDefaultScreenSaverDuration.InMinutes());
  }
}

AmbientController::AmbientController(
    mojo::PendingRemote<device::mojom::Fingerprint> fingerprint)
    : ambient_weather_controller_(std::make_unique<AmbientWeatherController>()),
      fingerprint_(std::move(fingerprint)) {
  ambient_backend_controller_ = CreateAmbientBackendController();

  // |SessionController| is initialized before |this| in Shell. Necessary to
  // bind observer here to monitor |OnActiveUserPrefServiceChanged|.
  session_observer_.Observe(Shell::Get()->session_controller());
  backlights_forced_off_observation_.Observe(
      Shell::Get()->backlights_forced_off_setter());
}

AmbientController::~AmbientController() {
  CloseUi(/*immediately=*/true);
}

void AmbientController::OnAmbientUiVisibilityChanged(
    AmbientUiVisibility visibility) {
  switch (visibility) {
    case AmbientUiVisibility::kShown:
      // Cancels the timer upon shown.
      inactivity_timer_.Stop();

      if (ash::features::IsScreenSaverDurationEnabled()) {
        StartTimerToReleaseWakeLock();
      } else if (IsChargerConnected()) {
        // Requires wake lock to prevent display from sleeping.
        AcquireWakeLock();
      }
      // Observes the |PowerStatus| on the battery charging status change for
      // the current ambient session.
      if (!power_status_observer_.IsObserving())
        power_status_observer_.Observe(PowerStatus::Get());

      MaybeStartScreenSaver();
      break;
    case AmbientUiVisibility::kPreview: {
      MaybeStartScreenSaver();
      break;
    }
    case AmbientUiVisibility::kHidden:
    case AmbientUiVisibility::kClosed: {
      // TODO(wutao): This will clear the image cache currently. It will not
      // work with `kHidden` if the token has expired and ambient mode is shown
      // again.
      StopScreensaver();

      // Should do nothing if the wake lock has already been released.
      ReleaseWakeLock();

      if (ash::features::IsScreenSaverDurationEnabled()) {
        screensaver_running_timer_.Stop();
      }

      Shell::Get()->RemovePreTargetHandler(this);

      // Should stop observing AssistantInteractionModel when ambient screen is
      // not shown.
      AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);

      if (visibility == AmbientUiVisibility::kHidden) {
        if (LockScreen::HasInstance()) {
          // Add observer for user activity.
          if (!user_activity_observer_.IsObserving())
            user_activity_observer_.Observe(ui::UserActivityDetector::Get());

          // Start timer to show ambient mode.
          inactivity_timer_.Start(
              FROM_HERE, ambient_ui_model_.lock_screen_inactivity_timeout(),
              base::BindOnce(&AmbientController::OnAutoShowTimeOut,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(visibility == AmbientUiVisibility::kClosed);
        inactivity_timer_.Stop();
        user_activity_observer_.Reset();
        power_status_observer_.Reset();
      }

      break;
    }
  }
}

void AmbientController::OnAutoShowTimeOut() {
  DCHECK(IsUiHidden(ambient_ui_model_.ui_visibility()));

  // Show ambient screen after time out.
  ShowUi();
}

void AmbientController::OnLoginOrLockScreenCreated() {
  if (!LockScreen::HasInstance() ||
      LockScreen::Get()->screen_type() != LockScreen::ScreenType::kLogin ||
      !IsAmbientModeManagedScreensaverEnabled() ||
      ambient_ui_model_.ui_visibility() != AmbientUiVisibility::kClosed) {
    return;
  }

  ShowHiddenUi();
}

void AmbientController::OnLockStateChanged(bool locked) {
  if (!locked) {
    // Ambient screen will be destroyed along with the lock screen when user
    // logs in.
    CloseUi();
    return;
  }

  if (!IsAmbientModeEnabled()) {
    VLOG(1) << "Ambient mode is not allowed.";
    return;
  }

  // Reset image failures to allow retrying ambient mode after lock state
  // changes.
  if (GetAmbientBackendModel()) {
    GetAmbientBackendModel()->ResetImageFailures();
  }

  // We have 3 options to manage the token for lock screen. Here use option 1.
  // 1. Request only one time after entering lock screen. We will use it once
  //    to request all the image links and no more requests.
  // 2. Request one time before entering lock screen. This will introduce
  //    extra latency.
  // 3. Request and refresh the token in the background (even the ambient mode
  //    is not started) with extra buffer time to use. When entering
  //    lock screen, it will be most likely to have the token already and
  //    enough time to use. More specifically,
  //    3a. We will leave enough buffer time (e.g. 10 mins before expire) to
  //        start to refresh the token.
  //    3b. When lock screen is triggered, most likely we will have >10 mins
  //        of token which can be used on lock screen.
  //    3c. There is a corner case that we may not have the token fetched when
  //        locking screen, we probably can use PrepareForLock(callback) when
  //        locking screen. We can add the refresh token into it. If the token
  //        has already been fetched, then there is not additional time to
  //        wait.
  RequestAccessToken(base::DoNothing(), /*may_refresh_token_on_lock=*/true);

  if (!IsShown()) {
    // When lock screen starts, we don't immediately show the UI. The Ui is
    // hidden and will show after a delay.
    ShowHiddenUi();
  }
}

void AmbientController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (GetPrimaryUserPrefService() != pref_service) {
    return;
  }

  // Do not continue if pref_change_registrar has already been set up. This
  // prevents re-binding observers when secondary profiles are activated.
  if (pref_change_registrar_)
    return;

  // Once logged in just remove the sign in pref registrations. So that we
  // don't react to device policy changes. Note: we do not need to re-add it
  // on logout because the chrome process is destroyed on logout.
  if (sign_in_pref_change_registrar_) {
    sign_in_pref_change_registrar_.reset();
  }

  bool ambient_mode_allowed = AmbientClient::Get()->IsAmbientModeAllowed();
  bool managed_screensaver_flag_enabled =
      ash::features::IsAmbientModeManagedScreensaverEnabled();

  if (ambient_mode_allowed || managed_screensaver_flag_enabled) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);
  }

  if (ambient_mode_allowed) {
    pref_change_registrar_->Add(
        ambient::prefs::kAmbientModeEnabled,
        base::BindRepeating(&AmbientController::OnEnabledPrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    OnEnabledPrefChanged();
  }

  if (managed_screensaver_flag_enabled) {
    pref_change_registrar_->Add(
        ambient::prefs::kAmbientModeManagedScreensaverEnabled,
        base::BindRepeating(
            &AmbientController::OnManagedScreensaverEnabledPrefChanged,
            weak_ptr_factory_.GetWeakPtr()));

    OnManagedScreensaverEnabledPrefChanged();
  }
}

void AmbientController::OnSigninScreenPrefServiceInitialized(
    PrefService* pref_service) {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return;
  }

  // Do not re-create the registrars if any registrar exists. This is done
  // so that in case the user pref registrar already exists it takes
  // priority.
  if (sign_in_pref_change_registrar_ || pref_change_registrar_) {
    return;
  }

  sign_in_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  sign_in_pref_change_registrar_->Init(pref_service);

  sign_in_pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeManagedScreensaverEnabled,
      base::BindRepeating(
          &AmbientController::OnManagedScreensaverEnabledPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  OnManagedScreensaverEnabledPrefChanged();
}

void AmbientController::OnPowerStatusChanged() {
  if (ambient_ui_model_.ui_visibility() != AmbientUiVisibility::kShown ||
      ash::features::IsScreenSaverDurationEnabled()) {
    // No action needed if ambient screen is not shown.
    return;
  }

  if (IsChargerConnected()) {
    AcquireWakeLock();
  } else {
    ReleaseWakeLock();
  }
}

void AmbientController::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& idle_state) {
  DVLOG(1) << "ScreenIdleStateChanged: dimmed(" << idle_state.dimmed()
           << ") off(" << idle_state.off() << ")";

  if (!IsAmbientModeEnabled())
    return;

  is_screen_off_ = idle_state.off();

  if (idle_state.off()) {
    DVLOG(1) << "Screen is off, close ambient mode.";

    CloseUi(/*immediately=*/true);
    return;
  }

  if (idle_state.dimmed()) {
    // Do not show the UI if lockscreen is active. The inactivity monitor should
    // have activated ambient mode.
    if (LockScreen::HasInstance())
      return;

    // Do not show UI if loading images was unsuccessful.
    if (GetAmbientBackendModel() &&
        GetAmbientBackendModel()->ImageLoadingFailed()) {
      VLOG(1) << "Skipping ambient mode activation due to prior failure";
      GetAmbientBackendModel()->ResetImageFailures();
      return;
    }

    ShowUi();
    return;
  }

  if (LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    ShowHiddenUi();
  }
}

void AmbientController::OnBacklightsForcedOffChanged(bool forced_off) {
  if (forced_off) {
    CloseUi(/*immediately=*/true);
  }
  if (!forced_off && LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    ShowHiddenUi();
  }
}

void AmbientController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // If about to suspend, turn everything off. This covers:
  //   1. Clicking power button.
  //   2. Close lid.
  // Need to specially close the widget immediately here to be able to close
  // the UI before device goes to suspend. Otherwise when opening lid after
  // lid closed, there may be a flash of the old window before previous
  // closing finished.
  CloseUi(/*immediately=*/true);
  is_suspend_imminent_ = true;
}

void AmbientController::SuspendDone(base::TimeDelta sleep_duration) {
  is_suspend_imminent_ = false;
  // |DismissUI| will restart the lock screen timer if lock screen is active and
  // if Ambient mode is enabled, so call it when resuming from suspend to
  // restart Ambient mode if applicable.
  DismissUI();
}

void AmbientController::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr msg,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  DismissUI();
}

void AmbientController::OnUserActivity(const ui::Event* event) {
  // The following events are handled separately so that we can consume them.
  // In case events come from external sources (i.e. Chrome extensions), the
  // event will be nullptr.
  if (IsShown() && event &&
      (event->IsMouseEvent() || event->IsTouchEvent() || event->IsKeyEvent() ||
       event->IsFlingScrollEvent())) {
    return;
  }
  // While |kPreview| is loading, don't |DismissUI| on user activity.
  // Users can still |DismissUI| with mouse, touch, key or assistant events.
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview &&
      !Shell::GetPrimaryRootWindowController()->HasAmbientWidget()) {
    return;
  }
  DismissUI();
}

void AmbientController::OnKeyEvent(ui::KeyEvent* event) {
  // Prevent dispatching key press event to the login UI.
  event->StopPropagation();
  // |DismissUI| only on |ET_KEY_PRESSED|. Otherwise it won't be possible to
  // start the preview by pressing "enter" key. It'll be cancelled immediately
  // on |ET_KEY_RELEASED|.
  if (event->type() == ui::ET_KEY_PRESSED) {
    DismissUI();
  }
}

void AmbientController::OnMouseEvent(ui::MouseEvent* event) {
  // |DismissUI| on actual mouse move only if the screen saver widget is shown
  // (images are downloaded).
  if (event->type() == ui::ET_MOUSE_MOVED) {
    MaybeDismissUIOnMouseMove();
    last_mouse_event_was_move_ = true;
    return;
  }

  // Prevent dispatching mouse event to the windows behind screen saver.
  // Let move event pass through, so that it clears hover states.
  event->StopPropagation();
  if (event->IsAnyButton()) {
    DismissUI();
  }
  last_mouse_event_was_move_ = false;
}

void AmbientController::OnTouchEvent(ui::TouchEvent* event) {
  // Prevent dispatching touch event to the windows behind screen saver.
  event->StopPropagation();
  DismissUI();
}

void AmbientController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kActive) {
    // Assistant is active.
    DismissUI();
  }
}

void AmbientController::ShowUi() {
  DVLOG(1) << __func__;

  // TODO(meilinw): move the eligibility check to the idle entry point once
  // implemented: b/149246117.
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  if (is_suspend_imminent_) {
    VLOG(1) << "Do not show UI when suspend imminent";
    return;
  }

  // If the ambient ui launcher is not ready to be started then do not change
  // the visibility. This will disabled the ui launcher until the next AmbientUi
  // starting event occurs. Right now the only ambient ui starting events are
  // screen lock/unlock, screen dim, preview and screen backlight off.
  if (ambient_ui_launcher_ && !ambient_ui_launcher_->IsReady()) {
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
}

void AmbientController::StartScreenSaverPreview() {
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kPreview);
  base::RecordAction(base::UserMetricsAction(kScreenSaverPreviewUserAction));
}

void AmbientController::ShowHiddenUi() {
  DVLOG(1) << __func__;

  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  if (is_suspend_imminent_) {
    VLOG(1) << "Do not start hidden UI when suspend imminent";
    return;
  }

  if (is_screen_off_) {
    VLOG(1) << "Do not start hidden UI when screen is off";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
}

void AmbientController::CloseUi(bool immediately) {
  DVLOG(1) << __func__;

  close_widgets_immediately_ = immediately;
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
  if (!Shell::Get()->IsInTabletMode()) {
    Shell::Get()->cursor_manager()->ShowCursor();
  }
}

void AmbientController::ToggleInSessionUi() {
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed)
    ShowUi();
  else
    CloseUi();
}

void AmbientController::SetScreenSaverDuration(int minutes) {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service) {
    return;
  }
  pref_service->Set(ambient::prefs::kAmbientModeRunningDurationMinutes,
                    base::Value(minutes));
}

absl::optional<int> AmbientController::GetScreenSaverDuration() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service) {
    return absl::nullopt;
  }
  return pref_service->GetInteger(
      ambient::prefs::kAmbientModeRunningDurationMinutes);
}

void AmbientController::StartTimerToReleaseWakeLock() {
  AcquireWakeLock();

  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service) {
    return;
  }

  const int session_duration_in_minutes = pref_service->GetInteger(
      ambient::prefs::kAmbientModeRunningDurationMinutes);
  DCHECK(session_duration_in_minutes >= 0);

  if (session_duration_in_minutes != kDurationForever) {
    const base::TimeDelta delay =
        base::Minutes(session_duration_in_minutes) - kReleaseWakeLockDelay;
    screensaver_running_timer_.Start(FROM_HERE, delay, this,
                                     &AmbientController::ReleaseWakeLock);
  }
}

bool AmbientController::IsShown() const {
  return ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kShown ||
         ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview;
}

void AmbientController::AcquireWakeLock() {
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> provider;
    AmbientClient::Get()->RequestWakeLockProvider(
        provider.BindNewPipeAndPassReceiver());
    provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, kWakeLockReason,
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  DCHECK(wake_lock_);
  wake_lock_->RequestWakeLock();
  VLOG(1) << "Acquired wake lock";

  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->CanLockScreen() &&
      session_controller->ShouldLockScreenAutomatically()) {
    if (!session_controller->IsScreenLocked() &&
        !delayed_lock_timer_.IsRunning()) {
      delayed_lock_timer_.Start(
          FROM_HERE, ambient_ui_model_.background_lock_screen_timeout(),
          base::BindOnce(
              []() { Shell::Get()->session_controller()->LockScreen(); }));
    }
  }
}

void AmbientController::ReleaseWakeLock() {
  if (!wake_lock_)
    return;

  wake_lock_->CancelWakeLock();
  VLOG(1) << "Released wake lock";

  delayed_lock_timer_.Stop();
}

void AmbientController::CloseAllWidgets(bool immediately) {
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CloseAmbientWidget(immediately);
  }
}

PrefChangeRegistrar* AmbientController::GetActivePrefChangeRegistrar() {
  if (pref_change_registrar_) {
    return pref_change_registrar_.get();
  }

  if (ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return sign_in_pref_change_registrar_.get();
  }
  return nullptr;
}

void AmbientController::AddManagedScreensaverPolicyPrefObservers() {
  PrefChangeRegistrar* registrar = GetActivePrefChangeRegistrar();
  CHECK(registrar);
  registrar->Add(
      ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::
              OnManagedScreensaverLockScreenIdleTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  registrar->Add(
      ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
      base::BindRepeating(
          &AmbientController::
              OnManagedScreensaverPhotoRefreshIntervalPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  OnManagedScreensaverLockScreenIdleTimeoutPrefChanged();
  OnManagedScreensaverPhotoRefreshIntervalPrefChanged();
}

void AmbientController::RemoveAmbientModeSettingsPrefObservers() {
  for (const auto* pref_name :
       {ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
        ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
        ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
        ambient::prefs::kAmbientUiSettings,
        ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
        ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
        ambient::prefs::
            kAmbientModeManagedScreensaverImageDisplayIntervalSeconds}) {
    if (pref_change_registrar_ &&
        pref_change_registrar_->IsObserved(pref_name)) {
      pref_change_registrar_->Remove(pref_name);
    }
    if (sign_in_pref_change_registrar_ &&
        sign_in_pref_change_registrar_->IsObserved(pref_name)) {
      sign_in_pref_change_registrar_->Remove(pref_name);
    }
  }
}

void AmbientController::OnManagedScreensaverLockScreenIdleTimeoutPrefChanged() {
  PrefService* pref_service = GetActivePrefService();
  CHECK(pref_service);
  ambient_ui_model_.SetLockScreenInactivityTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds)));
}

void AmbientController::OnManagedScreensaverPhotoRefreshIntervalPrefChanged() {
  PrefService* pref_service = GetActivePrefService();
  CHECK(pref_service);
  ambient_ui_model_.SetPhotoRefreshInterval(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::
              kAmbientModeManagedScreensaverImageDisplayIntervalSeconds)));
}

void AmbientController::OnManagedScreensaverEnabledPrefChanged() {
  ResetAmbientControllerResources();
  OnEnabledPrefChanged();

  if (!IsAmbientModeManagedScreensaverEnabled()) {
    return;
  }
  RemoveAmbientModeSettingsPrefObservers();
  AddManagedScreensaverPolicyPrefObservers();
  if (!LockScreen::HasInstance()) {
    return;
  }
  // Start hidden ambient mode immediately if the lock screen has an instance
  // and ambient mode is enabled.
  ShowHiddenUi();
}

void AmbientController::AddAmbientModeUserSettingsPolicyPrefObservers() {
  // Note: in case we ever want to enable the consumer screensaver on the
  // login screen we should change the pref_change_registrar here with
  // `GetActivePrefChangeRegistrar()` and the corresponding
  // `GetPrimaryUserPrefService()` with `GetActivePrefService()` in the actual
  // method calls.
  if (!pref_change_registrar_) {
    return;
  }

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::OnLockScreenInactivityTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::OnLockScreenBackgroundTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
      base::BindRepeating(&AmbientController::OnPhotoRefreshIntervalPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientUiSettings,
      base::BindRepeating(&AmbientController::OnAmbientUiSettingsChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
      base::BindRepeating(&AmbientController::OnAnimationPlaybackSpeedChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Trigger the callbacks manually the first time to init AmbientUiModel.
  OnLockScreenInactivityTimeoutPrefChanged();
  OnLockScreenBackgroundTimeoutPrefChanged();
  OnPhotoRefreshIntervalPrefChanged();
  OnAnimationPlaybackSpeedChanged();
}

void AmbientController::OnEnabledPrefChanged() {
  if (IsAmbientModeEnabled()) {
    if (is_initialized_) {
      LOG(WARNING) << "Ambient mode is already enabled";
      return;
    }
    DVLOG(1) << "Ambient mode enabled";

    AddAmbientModeUserSettingsPolicyPrefObservers();

    photo_cache_ = AmbientPhotoCache::Create(
        GetCacheRootPath().Append(
            FILE_PATH_LITERAL(kAmbientModeCacheDirectoryName)),
        *AmbientClient::Get(), access_token_controller_);
    backup_photo_cache_ = AmbientPhotoCache::Create(
        GetCacheRootPath().Append(
            FILE_PATH_LITERAL(kAmbientModeBackupCacheDirectoryName)),
        *AmbientClient::Get(), access_token_controller_);
    CreateUiLauncher();

    ambient_ui_model_observer_.Observe(&ambient_ui_model_);
    auto* power_manager_client = chromeos::PowerManagerClient::Get();
    DCHECK(power_manager_client);
    power_manager_client_observer_.Observe(power_manager_client);

    fingerprint_->AddFingerprintObserver(
        fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

    ambient_animation_progress_tracker_ =
        std::make_unique<AmbientAnimationProgressTracker>();

    is_initialized_ = true;
  } else {
    DVLOG(1) << "Ambient mode disabled";
    ResetAmbientControllerResources();
  }
}

void AmbientController::ResetAmbientControllerResources() {
  CloseUi();

  ambient_animation_progress_tracker_.reset();

  RemoveAmbientModeSettingsPrefObservers();

  ambient_ui_model_observer_.Reset();
  power_manager_client_observer_.Reset();

  DestroyUiLauncher();
  backup_photo_cache_.reset();
  photo_cache_.reset();

  if (fingerprint_observer_receiver_.is_bound()) {
    fingerprint_observer_receiver_.reset();
  }
  is_initialized_ = false;
}

void AmbientController::OnLockScreenInactivityTimeoutPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetLockScreenInactivityTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds)));
}

void AmbientController::OnLockScreenBackgroundTimeoutPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetBackgroundLockScreenTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds)));
}

void AmbientController::OnPhotoRefreshIntervalPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetPhotoRefreshInterval(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds)));
}

void AmbientController::OnAmbientUiSettingsChanged() {
  DVLOG(4) << "AmbientUiSettings changed to "
           << GetCurrentUiSettings().ToString();
  // For a given topic category, the topics downloaded from IMAX and saved to
  // cache differ from theme to theme:
  // 1) Slideshow mode keeps primary/related photos paired within a topic,
  //    whereas animated themes split the photos into 2 separate topics.
  // 2) The resolution of the photos downloaded from FIFE may differ between
  //    themes, depending on the image assets' sizes in the animation file.
  // For this reason, it is better to not re-use the cache when switching
  // between themes.
  //
  // There are corner cases here where the theme may change and the program
  // crashes before the cache gets cleared below. This is intentionally not
  // accounted for because it's not worth the added complexity. If this
  // should happen, re-using the cache will still work without fatal behavior.
  // The UI may just not be optimal. Furthermore, the cache gradually gets
  // overwritten with topics reflecting the new theme anyways, so ambient mode
  // should not be stuck with a mismatched cache indefinitely.
  CHECK(photo_cache_);
  photo_cache_->Clear();

  // The |AmbientUiLauncher| implementation to use is largely dependent on
  // the current |AmbientUiSettings|, so this needs to be recreated.
  CreateUiLauncher();
}

void AmbientController::OnAnimationPlaybackSpeedChanged() {
  DCHECK(GetPrimaryUserPrefService());
  ambient_ui_model_.set_animation_playback_speed(
      GetPrimaryUserPrefService()->GetDouble(
          ambient::prefs::kAmbientModeAnimationPlaybackSpeed));
}

void AmbientController::RequestAccessToken(
    AmbientAccessTokenController::AccessTokenCallback callback,
    bool may_refresh_token_on_lock) {
  // Do not request access tokens when the ambient mode is in the managed mode
  // as we do not want to rely on any user information .
  if (IsAmbientModeManagedScreensaverEnabled()) {
    // Consume the callback to be resilient against dependencies on the callback
    // in the future.
    std::move(callback).Run("", "");
    return;
  }
  access_token_controller_.RequestAccessToken(std::move(callback),
                                              may_refresh_token_on_lock);
}

void AmbientController::DismissUI() {
  if (!IsAmbientModeEnabled()) {
    CloseUi();
    return;
  }

  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kHidden) {
    // Double resetting crashes the UI, make sure it is running.
    if (inactivity_timer_.IsRunning()) {
      inactivity_timer_.Reset();
    }
    return;
  }

  if (LockScreen::HasInstance()) {
    ShowHiddenUi();
    return;
  }

  CloseUi();
}

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  if (ambient_ui_launcher_) {
    // This can legitimately be null. Some ambient UIs do not use photos at all
    // and hence, do not have an active |AmbientBackendModel|.
    // TODO(b/274164306): Move |AmbientBackendModel| references completely out
    // of |AmbientController|. The business logic should be migrated elsewhere
    // (likely somewhere within an |AmbientUiLauncher| implementation).
    return ambient_ui_launcher_->GetAmbientBackendModel();
  }

  DCHECK(ambient_photo_controller_);
  return ambient_photo_controller_->ambient_backend_model();
}

AmbientWeatherModel* AmbientController::GetAmbientWeatherModel() {
  return ambient_weather_controller_->weather_model();
}

void AmbientController::OnImagesReady() {
  CreateAndShowWidgets();
}

void AmbientController::OnImagesFailed() {
  LOG(ERROR) << "Ambient mode failed to start";
  CloseUi();
}

std::unique_ptr<views::Widget> AmbientController::CreateWidget(
    aura::Window* container) {
  AmbientTheme current_theme = GetCurrentUiSettings().theme();
  std::unique_ptr<AmbientContainerView> container_view;
  if (ambient_ui_launcher_) {
    container_view = std::make_unique<AmbientContainerView>(
        current_theme, ambient_ui_launcher_->CreateView(),
        session_metrics_recorder_.get());
  } else {
    // TODO(b/274164306): Everything should use
    // |AmbientUiLauncher::CreateView()| when slideshow and animation themes
    // are migrated to AmbientUiLauncher.
    container_view = std::make_unique<AmbientContainerView>(
        &delegate_, ambient_animation_progress_tracker_.get(),
        AmbientAnimationStaticResources::Create(current_theme,
                                                /*serializable=*/true),
        session_metrics_recorder_.get(), frame_rate_controller_.get());
  }
  auto* widget_delegate = new AmbientWidgetDelegate();
  widget_delegate->SetInitiallyFocusedView(container_view.get());

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = GetWidgetName();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.parent = container;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = widget_delegate;
  params.visible_on_all_workspaces = true;

  // Do not change the video wake lock.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  auto* contents_view = widget->SetContentsView(std::move(container_view));

  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_BOTH);
  ::wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(), ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityChangesAnimated(widget->GetNativeWindow());

  widget->Show();

  // Only announce for the primary window.
  if (Shell::GetPrimaryRootWindow() == container->GetRootWindow()) {
    contents_view->GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_ASH_SCREENSAVER_STARTS));
  }

  return widget;
}

void AmbientController::OnUiLauncherInitialized(bool success) {
  if (!success) {
    // Success = false denotes a case where the screensaver is in a permanent
    // error state and such that the UI and any further attempts to launch the
    // UI will also result in this failure.
    // TODO (b/175142676) Add metrics for cases where success = false.
    LOG(ERROR) << "AmbientUiLauncher failed to initialize";
    return;
  }
  CreateAndShowWidgets();
}

void AmbientController::CreateAndShowWidgets() {
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview) {
    preview_widget_created_at_ = base::Time::Now();
  }
  // Hide cursor.
  Shell::Get()->cursor_manager()->HideCursor();
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CreateAmbientWidget();
  }
}

void AmbientController::StartRefreshingImages() {
  DCHECK(ambient_photo_controller_);
  // There is no use case for switching themes "on-the-fly" while ambient mode
  // is rendering. Thus, it's sufficient to just reinitialize the
  // model/controller with the appropriate config each time before calling
  // StartScreenUpdate().
  DCHECK(!ambient_photo_controller_->IsScreenUpdateActive());
  AmbientTheme current_theme = GetCurrentUiSettings().theme();
  DVLOG(4) << "Loaded ambient theme " << ToString(current_theme);

  AmbientPhotoConfig photo_config;
  std::unique_ptr<AmbientTopicQueue::Delegate> topic_queue_delegate;
  if (current_theme == AmbientTheme::kSlideshow) {
    photo_config = CreateAmbientSlideshowPhotoConfig();
    topic_queue_delegate =
        std::make_unique<AmbientTopicQueueSlideshowDelegate>();
  } else {
    scoped_refptr<cc::SkottieWrapper> animation =
        AmbientAnimationStaticResources::Create(current_theme,
                                                /*serializable=*/false)
            ->GetSkottieWrapper();
    photo_config =
        CreateAmbientAnimationPhotoConfig(animation->GetImageAssetMetadata());
    topic_queue_delegate = std::make_unique<AmbientTopicQueueAnimationDelegate>(
        animation->GetImageAssetMetadata());
  }
  ambient_photo_controller_->ambient_backend_model()->SetPhotoConfig(
      std::move(photo_config));
  ambient_photo_controller_->StartScreenUpdate(std::move(topic_queue_delegate));
}

void AmbientController::StopScreensaver() {
  CloseAllWidgets(close_widgets_immediately_);
  frame_rate_controller_.reset();
  session_metrics_recorder_.reset();

  if (ambient_ui_launcher_) {
    ambient_ui_launcher_->Finalize();
    return;
  }
  weather_refresher_.reset();
  DCHECK(ambient_photo_controller_);
  ambient_photo_controller_->StopScreenUpdate();
}

void AmbientController::MaybeStartScreenSaver() {
  // The screensaver may have already been started.
  if (IsUiLauncherActive()) {
    return;
  }

  if (!user_activity_observer_.IsObserving())
    user_activity_observer_.Observe(ui::UserActivityDetector::Get());

  // Add observer for assistant interaction model
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);

  session_metrics_recorder_ = std::make_unique<AmbientSessionMetricsRecorder>(
      GetCurrentUiSettings().theme());
  frame_rate_controller_ =
      std::make_unique<AmbientAnimationFrameRateController>(
          Shell::Get()->frame_throttling_controller());

  Shell::Get()->AddPreTargetHandler(this);
  if (ambient_ui_launcher_) {
    ambient_ui_launcher_->Initialize(
        base::BindOnce(&AmbientController::OnUiLauncherInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartRefreshingImages();
    // TODO(b/274164306): Move `weather_refresher_` to `AmbientUiLauncher`
    // implementation for slideshow and animation themes.
    weather_refresher_ = ambient_weather_controller_->CreateScopedRefresher();
  }
}

AmbientUiSettings AmbientController::GetCurrentUiSettings() const {
  CHECK(GetActivePrefService());
  return AmbientUiSettings::ReadFromPrefService(*GetActivePrefService());
}

void AmbientController::MaybeDismissUIOnMouseMove() {
  // If the move was not an actual mouse move event or the screen saver widget
  // is not shown yet (images are not downloaded), don't dismiss.
  if (!last_mouse_event_was_move_ ||
      !Shell::GetPrimaryRootWindowController()->HasAmbientWidget()) {
    return;
  }

  // In preview mode, don't dismiss until the timer stops running (avoids
  // accidental dismissal).
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview) {
    auto elapsed = base::Time::Now() - preview_widget_created_at_;
    if (elapsed < kDismissPreviewOnMouseMoveDelay) {
      return;
    }
  }
  DismissUI();
}

void AmbientController::CreateUiLauncher() {
  if (IsUiLauncherActive()) {
    // There are no known use cases where the AmbientUiSettings selected by the
    // user can change while in the middle of an ambient session, but this is
    // handled gracefully just in case.
    LOG(DFATAL) << "Cannot reset the AmbientUiLauncher while it is active";
    return;
  }

  DestroyUiLauncher();

  if (IsAmbientModeManagedScreensaverEnabled()) {
    ambient_ui_launcher_ =
        std::make_unique<AmbientManagedSlideshowUiLauncher>(&delegate_);
  } else if (GetCurrentUiSettings().theme() == AmbientTheme::kVideo) {
    ambient_ui_launcher_ = std::make_unique<AmbientVideoUiLauncher>(
        GetPrimaryUserPrefService(), &delegate_);
  } else {
    // TODO(b/274164306): Remove when slideshow and animation themes are
    // migrated to AmbientUiLauncher.
    CHECK(photo_cache_);
    CHECK(backup_photo_cache_);
    ambient_photo_controller_ = std::make_unique<AmbientPhotoController>(
        *photo_cache_, *backup_photo_cache_, delegate_,
        // The type of photo config specified here is actually irrelevant as
        // it always gets reset with the correct configuration anyways in
        // StartRefreshingImages() before ambient mode starts.
        CreateAmbientSlideshowPhotoConfig());
    // The new UiLauncher API adds backend model observers in its
    // implementation and thus the observer is not required when using the new
    // codepath.
    // TODO(esum) Get rid the ambient_backend_model_observer_ and
    // corresponding methods once other photo controllers are migrated to the
    // new API.
    ambient_backend_model_observer_.Observe(GetAmbientBackendModel());
  }
}

void AmbientController::DestroyUiLauncher() {
  ambient_ui_launcher_.reset();
  // TODO(b/274164306): Remove when slideshow and animation themes are migrated
  // to AmbientUiLauncher.
  ambient_backend_model_observer_.Reset();
  ambient_photo_controller_.reset();
}

bool AmbientController::IsUiLauncherActive() const {
  return (ambient_ui_launcher_ && ambient_ui_launcher_->IsActive()) ||
         // TODO(b/274164306): Remove when slideshow and animation themes are
         // migrated to AmbientUiLauncher.
         (ambient_photo_controller_ &&
          ambient_photo_controller_->IsScreenUpdateActive());
}

}  // namespace ash
