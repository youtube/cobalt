// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/keyboard_capability_delegate_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"

namespace ash {

KeyboardCapabilityDelegateImpl::KeyboardCapabilityDelegateImpl() {
  Shell::Get()->session_controller()->AddObserver(this);
}

KeyboardCapabilityDelegateImpl::~KeyboardCapabilityDelegateImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void KeyboardCapabilityDelegateImpl::AddObserver(
    ui::KeyboardCapability::Observer* observer) {
  observers_.AddObserver(observer);
}

void KeyboardCapabilityDelegateImpl::RemoveObserver(
    ui::KeyboardCapability::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool KeyboardCapabilityDelegateImpl::TopRowKeysAreFKeys() const {
  // Handle the case when top_row_are_f_keys_pref_ is not initialized yet.
  if (top_row_are_f_keys_pref_->GetPrefName().empty()) {
    return false;
  }
  return top_row_are_f_keys_pref_->GetValue();
}

void KeyboardCapabilityDelegateImpl::SetTopRowKeysAsFKeysEnabledForTesting(
    bool enabled) {
  top_row_are_f_keys_pref_->SetValue(enabled);
}

bool KeyboardCapabilityDelegateImpl::IsPrivacyScreenSupported() const {
  if (is_privacy_screen_supported_for_testing_) {
    CHECK_IS_TEST();
    return *is_privacy_screen_supported_for_testing_;
  }

  return Shell::Get()->privacy_screen_controller() &&
         Shell::Get()->privacy_screen_controller()->IsSupported();
}

void KeyboardCapabilityDelegateImpl::SetPrivacyScreenSupportedForTesting(
    bool is_supported) {
  CHECK_IS_TEST();
  is_privacy_screen_supported_for_testing_ = is_supported;
}

void KeyboardCapabilityDelegateImpl::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  InitUserPrefs(prefs);
}

void KeyboardCapabilityDelegateImpl::InitUserPrefs(PrefService* prefs) {
  if (prefs && prefs->FindPreference(prefs::kSendFunctionKeys)) {
    top_row_are_f_keys_pref_ = std::make_unique<BooleanPrefMember>();
    top_row_are_f_keys_pref_->Init(
        prefs::kSendFunctionKeys, prefs,
        base::BindRepeating(
            &KeyboardCapabilityDelegateImpl::NotifyTopRowKeysAreFKeysChanged,
            base::Unretained(this)));
  }
}

void KeyboardCapabilityDelegateImpl::NotifyTopRowKeysAreFKeysChanged() {
  for (auto& observer : observers_) {
    observer.OnTopRowKeysAreFKeysChanged();
  }
}

}  // namespace ash
