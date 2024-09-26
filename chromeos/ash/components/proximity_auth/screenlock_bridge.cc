// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

#include <string>
#include <utility>

#include <memory>

#include "base/check_is_test.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace proximity_auth {
namespace {

base::LazyInstance<ScreenlockBridge>::DestructorAtExit
    g_screenlock_bridge_instance = LAZY_INSTANCE_INITIALIZER;

// Ids for the icons that are supported by lock screen and signin screen
// account picker as user pod custom icons.
// The id's should be kept in sync with values used by user_pod_row.js.
const char kLockedUserPodCustomIconId[] = "locked";
const char kLockedToBeActivatedUserPodCustomIconId[] = "locked-to-be-activated";
const char kLockedWithProximityHintUserPodCustomIconId[] =
    "locked-with-proximity-hint";
const char kUnlockedUserPodCustomIconId[] = "unlocked";
const char kSpinnerUserPodCustomIconId[] = "spinner";

// Given the user pod icon, returns its id as used by the user pod UI code.
std::string GetIdForIcon(ScreenlockBridge::UserPodCustomIcon icon) {
  switch (icon) {
    case ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED:
      return kLockedUserPodCustomIconId;
    case ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED:
      return kLockedToBeActivatedUserPodCustomIconId;
    case ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT:
      return kLockedWithProximityHintUserPodCustomIconId;
    case ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED:
      return kUnlockedUserPodCustomIconId;
    case ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER:
      return kSpinnerUserPodCustomIconId;
    default:
      return "";
  }
}

}  // namespace

ScreenlockBridge::UserPodCustomIconInfo::UserPodCustomIconInfo() = default;

ScreenlockBridge::UserPodCustomIconInfo::~UserPodCustomIconInfo() = default;

base::Value::Dict ScreenlockBridge::UserPodCustomIconInfo::ToDictForTesting()
    const {
  CHECK_IS_TEST();
  auto result = base::Value::Dict();
  result.Set("id", GetIDString());

  if (!tooltip_.empty()) {
    base::Value::Dict tooltip_options;
    tooltip_options.Set("text", tooltip_);
    tooltip_options.Set("autoshow", autoshow_tooltip_);
    result.Set("tooltip", std::move(tooltip_options));
  }

  if (!aria_label_.empty())
    result.Set("ariaLabel", aria_label_);

  return result;
}

void ScreenlockBridge::UserPodCustomIconInfo::SetIcon(
    ScreenlockBridge::UserPodCustomIcon icon) {
  icon_ = icon;
}

void ScreenlockBridge::UserPodCustomIconInfo::SetTooltip(
    const std::u16string& tooltip,
    bool autoshow) {
  tooltip_ = tooltip;
  autoshow_tooltip_ = autoshow;
}

void ScreenlockBridge::UserPodCustomIconInfo::SetAriaLabel(
    const std::u16string& aria_label) {
  aria_label_ = aria_label;
}

std::string ScreenlockBridge::UserPodCustomIconInfo::GetIDString() const {
  return GetIdForIcon(icon_);
}

// static
ScreenlockBridge* ScreenlockBridge::Get() {
  return g_screenlock_bridge_instance.Pointer();
}

void ScreenlockBridge::SetLockHandler(LockHandler* lock_handler) {
  // Don't notify observers if there is no change -- i.e. if the screen was
  // already unlocked, and is remaining unlocked.
  if (lock_handler == lock_handler_)
    return;

  DCHECK(lock_handler_ == nullptr || lock_handler == nullptr);

  // TODO(isherman): If |lock_handler| is null, then |lock_handler_| might have
  // been freed. Cache the screen type rather than querying it below.
  LockHandler::ScreenType screen_type;
  if (lock_handler_)
    screen_type = lock_handler_->GetScreenType();
  else
    screen_type = lock_handler->GetScreenType();

  lock_handler_ = lock_handler;
  if (lock_handler_) {
    for (auto& observer : observers_)
      observer.OnScreenDidLock(screen_type);
  } else {
    focused_account_id_ = EmptyAccountId();
    for (auto& observer : observers_)
      observer.OnScreenDidUnlock(screen_type);
  }
}

void ScreenlockBridge::SetFocusedUser(const AccountId& account_id) {
  if (account_id == focused_account_id_)
    return;
  focused_account_id_ = account_id;
  for (auto& observer : observers_)
    observer.OnFocusedUserChanged(account_id);
}

bool ScreenlockBridge::IsLocked() const {
  return lock_handler_ != nullptr;
}

void ScreenlockBridge::Lock() {
  ash::SessionManagerClient::Get()->RequestLockScreen();
}

void ScreenlockBridge::Unlock(const AccountId& account_id) {
  if (lock_handler_)
    lock_handler_->Unlock(account_id);
}

void ScreenlockBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenlockBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ScreenlockBridge::ScreenlockBridge()
    : lock_handler_(nullptr), focused_account_id_(EmptyAccountId()) {}

ScreenlockBridge::~ScreenlockBridge() {}

}  // namespace proximity_auth
