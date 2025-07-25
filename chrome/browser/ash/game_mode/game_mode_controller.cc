// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/game_mode_controller.h"

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/views/widget/widget.h"

namespace game_mode {

using borealis::BorealisWindowManager;

namespace {

constexpr int kRefreshSec = 60;
constexpr int kTimeoutSec = kRefreshSec + 10;

}  // namespace

GameModeController::GameModeController() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  // In case a window is already focused when this is constructed.
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

GameModeController::~GameModeController() {
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
}

void GameModeController::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  auto maybe_keep_focused = std::move(focused_);

  if (!gained_focus)
    return;

  auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
  // |widget| can be nullptr in tests.
  if (!widget)
    return;

  aura::Window* window = widget->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  if (!window_state)
    return;

  if (BorealisWindowManager::IsBorealisWindow(window)) {
    focused_ = std::make_unique<WindowTracker>(
        window_state, std::move(maybe_keep_focused),
        base::BindRepeating(
            &GameModeController::NotifySetGameMode,
            // This is safe because the callback is only used by WindowTracker
            // and GameModeEnabler, which are owned by (or transitively owned
            // by) GameModeController. Therefore |this| cannot be stale.
            base::Unretained(this)));
  }
}

GameModeController::WindowTracker::WindowTracker(
    ash::WindowState* window_state,
    std::unique_ptr<WindowTracker> previous_focus,
    NotifySetGameModeCallback notify_set_game_mode_callback)
    : notify_set_game_mode_callback_(notify_set_game_mode_callback) {
  auto* window = window_state->window();

  // Maintain game mode state without dropping momentarily out of it
  // when switching between full-screen game windows.
  if (previous_focus) {
    // Enabler will be null if game mode is not on before focus switch.
    game_mode_enabler_ = std::move(previous_focus->game_mode_enabler_);
  }

  // This possibly turns OFF as well as turns on game mode.
  UpdateGameModeStatus(window_state);

  window_state_observer_.Observe(window_state);
  window_observer_.Observe(window);
}

GameModeController::WindowTracker::~WindowTracker() {}

void GameModeController::WindowTracker::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  UpdateGameModeStatus(window_state);
}

void GameModeController::WindowTracker::UpdateGameModeStatus(
    ash::WindowState* window_state) {
  auto* window = window_state->window();

  if (!window_state->IsFullscreen() ||
      !BorealisWindowManager::IsBorealisWindow(window)) {
    game_mode_enabler_.reset();
    return;
  }

  if (game_mode_enabler_) {
    // No need to create a new enabler. The existing one is already valid for
    // this window.
    return;
  }

  VLOG(2) << "Initializing GameModeEnabler for Borealis";

  // Borealis has no further criteria than the window being fullscreen and
  // focused, already guaranteed by WindowTracker existing.
  game_mode_enabler_ =
      std::make_unique<GameModeEnabler>(notify_set_game_mode_callback_);
}

void GameModeController::WindowTracker::OnWindowDestroying(
    aura::Window* window) {
  window_state_observer_.Reset();
  window_observer_.Reset();
  game_mode_enabler_.reset();
}

bool GameModeController::GameModeEnabler::should_record_failure;

GameModeController::GameModeEnabler::GameModeEnabler(
    NotifySetGameModeCallback notify_set_game_mode_callback)
    : notify_set_game_mode_callback_(notify_set_game_mode_callback) {
  notify_set_game_mode_callback_.Run(GameMode::BOREALIS);

  GameModeEnabler::should_record_failure = true;
  base::UmaHistogramEnumeration(kGameModeResultHistogramName,
                                GameModeResult::kAttempted);
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        GameMode::BOREALIS, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode,
                       /*refresh_of=*/absl::nullopt));
  }
  timer_.Start(FROM_HERE, base::Seconds(kRefreshSec), this,
               &GameModeEnabler::RefreshGameMode);
}

GameModeController::GameModeEnabler::~GameModeEnabler() {
  auto time_in_mode = began_.Elapsed();
  base::UmaHistogramLongTimes100(kTimeInGameModeHistogramName, time_in_mode);

  notify_set_game_mode_callback_.Run(GameMode::OFF);

  timer_.Stop();
  VLOG(1) << "Turning off game mode";
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        GameMode::OFF, 0,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, GameMode::BOREALIS));
  }
}

void GameModeController::GameModeEnabler::RefreshGameMode() {
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        GameMode::BOREALIS, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, GameMode::BOREALIS));
  }
}

// Previous is whether game mode was enabled previous to this call.
void GameModeController::GameModeEnabler::OnSetGameMode(
    absl::optional<GameMode> refresh_of,
    absl::optional<GameMode> previous) {
  if (!previous.has_value()) {
    LOG(ERROR) << "Failed to set Game Mode";
  } else if (GameModeEnabler::should_record_failure && refresh_of.has_value() &&
             previous.value() != refresh_of.value()) {
    // If game mode was not on and it was not the initial call,
    // it means the previous call failed/timed out.
    base::UmaHistogramEnumeration(kGameModeResultHistogramName,
                                  GameModeResult::kFailed);
    // Only record failures once per entry into gamemode.
    GameModeEnabler::should_record_failure = false;
  }
}

void GameModeController::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void GameModeController::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void GameModeController::NotifySetGameMode(GameMode game_mode) {
  for (auto& obs : observers_) {
    obs.OnSetGameMode(game_mode);
  }
}

}  // namespace game_mode
