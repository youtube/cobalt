// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow_controller.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/resize_shadow.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

namespace {

// Lock shadow params
constexpr ResizeShadow::InitParams kLockParams{
    /*thickness=*/6,
    /*shadow_corner_radius=*/6,
    /*window_corner_radius=*/2,
    /*opacity =*/0.3f,
    /*color=*/gfx::kGoogleGrey900,
    /*hit_test_enabled=*/false,
    /*hide_duration_ms=*/0,
};

}  // namespace

ResizeShadowController::ResizeShadowController() = default;

ResizeShadowController::~ResizeShadowController() {
  RemoveAllShadows();
}

void ResizeShadowController::ShowShadow(aura::Window* window, int hit_test) {
  RecreateShadowIfNeeded(window);
  if (ShouldShowShadowForWindow(window) && window->IsVisible())
    GetShadowForWindow(window)->ShowForHitTest(hit_test);
}

void ResizeShadowController::TryShowAllShadows() {
  for (const auto& shadow : window_shadows_)
    UpdateShadowVisibility(shadow.first, shadow.first->IsVisible());
}

void ResizeShadowController::HideShadow(aura::Window* window) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (!shadow)
    return;
  UpdateShadowVisibility(window, false);
}

void ResizeShadowController::HideAllShadows() {
  for (auto& shadow : window_shadows_) {
    if (!shadow.second)
      continue;

    switch (shadow.second->type_) {
      case ResizeShadowType::kLock: {  // Hides lock style of shadow
        UpdateShadowVisibility(shadow.first, false);
        break;
      }
      case ResizeShadowType::kUnlock: {  // Deletes unlock style of shadow
        shadow.second.reset();
        break;
      }
    }
  }
}

void ResizeShadowController::RemoveAllShadows() {
  windows_observation_.RemoveAllObservations();
  window_shadows_.clear();
}

void ResizeShadowController::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  ResizeShadow* shadow = GetShadowForWindow(params.target);
  if (shadow)
    shadow->ReparentLayer();
}

void ResizeShadowController::OnWindowVisibilityChanging(aura::Window* window,
                                                        bool visible) {
  UpdateShadowVisibility(window, visible);
}

void ResizeShadowController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow && window->GetProperty(aura::client::kUseWindowBoundsForShadow))
    shadow->UpdateBoundsAndVisibility();
}

void ResizeShadowController::OnWindowStackingChanged(aura::Window* window) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->ReparentLayer();
}

void ResizeShadowController::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  window_shadows_.erase(window);
}

void ResizeShadowController::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;
  UpdateShadowVisibility(window, window->IsVisible());
}

void ResizeShadowController::UpdateResizeShadowBoundsOfWindow(
    aura::Window* window,
    const gfx::Rect& bounds) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->UpdateBounds(bounds);
}

ResizeShadow* ResizeShadowController::GetShadowForWindowForTest(
    aura::Window* window) {
  return GetShadowForWindow(window);
}

void ResizeShadowController::RecreateShadowIfNeeded(aura::Window* window) {
  if (!windows_observation_.IsObservingSource(window))
    windows_observation_.AddObservation(window);
  ResizeShadow* shadow = GetShadowForWindow(window);
  const ash::ResizeShadowType type =
      window->GetProperty(ash::kResizeShadowTypeKey);

  // If the |window| has a resize shadow with the requested type, no need to
  // recreate it.
  if (shadow && shadow->type_ == type)
    return;

  ResizeShadow::InitParams params;
  if (type == ResizeShadowType::kLock)
    params = kLockParams;

  auto new_shadow = std::make_unique<ResizeShadow>(window, params, type);

  auto it = window_shadows_.find(window);
  if (it == window_shadows_.end())
    window_shadows_.insert(std::make_pair(window, std::move(new_shadow)));
  else
    it->second = std::move(new_shadow);
}

ResizeShadow* ResizeShadowController::GetShadowForWindow(
    aura::Window* window) const {
  auto it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : nullptr;
}

void ResizeShadowController::UpdateShadowVisibility(aura::Window* window,
                                                    bool visible) const {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (!shadow)
    return;

  if (shadow->type_ == ResizeShadowType::kLock) {
    visible &= ShouldShowShadowForWindow(window);
    if (visible)
      shadow->ShowForHitTest();
  }

  if (!visible)
    shadow->Hide();
}

bool ResizeShadowController::ShouldShowShadowForWindow(
    aura::Window* window) const {
  // Hide the shadow if it's a maximized/fullscreen/minimized window or the
  // overview mode is active.
  ui::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  return show_state != ui::SHOW_STATE_FULLSCREEN &&
         show_state != ui::SHOW_STATE_MAXIMIZED &&
         show_state != ui::SHOW_STATE_MINIMIZED &&
         !Shell::Get()->overview_controller()->InOverviewSession();
}

}  // namespace ash
