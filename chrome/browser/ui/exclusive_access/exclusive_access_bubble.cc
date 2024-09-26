// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "extensions/browser/extension_registry.h"
#include "ui/gfx/geometry/rect.h"

// NOTE(koz): Linux doesn't use the thick shadowed border, so we add padding
// here.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const int ExclusiveAccessBubble::kPaddingPx = 8;
#else
const int ExclusiveAccessBubble::kPaddingPx = 15;
#endif
const int ExclusiveAccessBubble::kInitialDelayMs = 3800;
const int ExclusiveAccessBubble::kIdleTimeMs = 2300;
const int ExclusiveAccessBubble::kSnoozeNotificationsTimeMs = 900000;  // 15m.
const int ExclusiveAccessBubble::kPositionCheckHz = 10;
const int ExclusiveAccessBubble::kSlideInRegionHeightPx = 4;
const int ExclusiveAccessBubble::kPopupTopPx = 45;
const int ExclusiveAccessBubble::kSimplifiedPopupTopPx = 45;

ExclusiveAccessBubble::ExclusiveAccessBubble(
    ExclusiveAccessManager* manager,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type)
    : manager_(manager),
      url_(url),
      bubble_type_(bubble_type),
      hide_timeout_(
          FROM_HERE,
          base::Milliseconds(kInitialDelayMs),
          base::BindRepeating(&ExclusiveAccessBubble::CheckMousePosition,
                              base::Unretained(this))),
      idle_timeout_(
          FROM_HERE,
          base::Milliseconds(kIdleTimeMs),
          base::BindRepeating(&ExclusiveAccessBubble::CheckMousePosition,
                              base::Unretained(this))),
      suppress_notify_timeout_(
          FROM_HERE,
          base::Milliseconds(kSnoozeNotificationsTimeMs),
          base::BindRepeating(&ExclusiveAccessBubble::CheckMousePosition,
                              base::Unretained(this))),
      mouse_position_checker_(
          FROM_HERE,
          base::Milliseconds(1000 / kPositionCheckHz),
          base::BindRepeating(&ExclusiveAccessBubble::CheckMousePosition,
                              base::Unretained(this))) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type_);
}

ExclusiveAccessBubble::~ExclusiveAccessBubble() = default;

void ExclusiveAccessBubble::OnUserInput() {
  // We got some user input; reset the idle timer.
  idle_timeout_.Reset();

  // If the notification suppression timer has elapsed, re-show it.
  if (!suppress_notify_timeout_.IsRunning()) {
    ShowAndStartTimers();
    return;
  }

  // The timer has not elapsed, but the user provided some input. Reset the
  // timer. (We only want to re-show the message after a period of inactivity.)
  suppress_notify_timeout_.Reset();
}

void ExclusiveAccessBubble::StartWatchingMouse() {
  // Start the initial delay timer and begin watching the mouse.
  ShowAndStartTimers();
  mouse_position_checker_.Reset();
}

void ExclusiveAccessBubble::StopWatchingMouse() {
  hide_timeout_.Stop();
  idle_timeout_.Stop();
  mouse_position_checker_.Stop();
}

bool ExclusiveAccessBubble::IsWatchingMouse() const {
  return mouse_position_checker_.IsRunning();
}

void ExclusiveAccessBubble::CheckMousePosition() {
  if (!hide_timeout_.IsRunning())
    Hide();
}

void ExclusiveAccessBubble::ExitExclusiveAccess() {
  manager_->ExitExclusiveAccess();
}

std::u16string ExclusiveAccessBubble::GetCurrentMessageText() const {
  return exclusive_access_bubble::GetLabelTextForType(
      bubble_type_, url_,
      extensions::ExtensionRegistry::Get(manager_->context()->GetProfile()));
}

std::u16string ExclusiveAccessBubble::GetCurrentDenyButtonText() const {
  return exclusive_access_bubble::GetDenyButtonTextForType(bubble_type_);
}

std::u16string ExclusiveAccessBubble::GetCurrentAllowButtonText() const {
  return exclusive_access_bubble::GetAllowButtonTextForType(bubble_type_, url_);
}

std::u16string ExclusiveAccessBubble::GetInstructionText(
    const std::u16string& accelerator) const {
  return exclusive_access_bubble::GetInstructionTextForType(
      bubble_type_, accelerator, notify_download_, notify_overridden_);
}

bool ExclusiveAccessBubble::IsHideTimeoutRunning() const {
  return hide_timeout_.IsRunning();
}

void ExclusiveAccessBubble::ShowAndStartTimers() {
  Show();

  // Do not allow the notification to hide for a few seconds.
  hide_timeout_.Reset();

  // Do not show the notification again until a long time has elapsed.
  suppress_notify_timeout_.Reset();
}
