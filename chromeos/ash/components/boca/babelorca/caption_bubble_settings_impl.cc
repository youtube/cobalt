// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::babelorca {

CaptionBubbleSettingsImpl::CaptionBubbleSettingsImpl(
    PrefService* profile_prefs,
    std::string_view caption_language_code)
    : profile_prefs_(profile_prefs),
      caption_language_code_(caption_language_code) {}

CaptionBubbleSettingsImpl::~CaptionBubbleSettingsImpl() = default;

void CaptionBubbleSettingsImpl::SetObserver(
    base::WeakPtr<::captions::CaptionBubbleSettings::Observer> observer) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kTranslateTargetLanguageCode,
      base::BindRepeating(&CaptionBubbleSettings::Observer::
                              OnLiveTranslateTargetLanguageChanged,
                          observer));
  observer_ = observer;
}

void CaptionBubbleSettingsImpl::RemoveObserver() {
  pref_change_registrar_.reset();
  observer_.reset();
}

bool CaptionBubbleSettingsImpl::IsLiveTranslateFeatureEnabled() {
  return translate_enabled_;
}

bool CaptionBubbleSettingsImpl::GetLiveCaptionBubbleExpanded() {
  return profile_prefs_->GetBoolean(prefs::kCaptionBubbleExpanded);
}

bool CaptionBubbleSettingsImpl::GetLiveTranslateEnabled() {
  return translate_enabled_;
}

std::string CaptionBubbleSettingsImpl::GetLiveCaptionLanguageCode() {
  return caption_language_code_;
}

std::string CaptionBubbleSettingsImpl::GetLiveTranslateTargetLanguageCode() {
  return profile_prefs_->GetString(prefs::kTranslateTargetLanguageCode);
}

void CaptionBubbleSettingsImpl::SetLiveCaptionEnabled(bool enabled) {}

void CaptionBubbleSettingsImpl::SetLiveCaptionBubbleExpanded(bool expanded) {
  profile_prefs_->SetBoolean(prefs::kCaptionBubbleExpanded, expanded);
}

void CaptionBubbleSettingsImpl::SetLiveTranslateTargetLanguageCode(
    std::string_view language_code) {
  profile_prefs_->SetString(prefs::kTranslateTargetLanguageCode, language_code);
}

void CaptionBubbleSettingsImpl::SetLiveTranslateEnabled(bool enabled) {
  bool enabled_changed = translate_enabled_ != enabled;
  translate_enabled_ = enabled;
  if (!enabled_changed || !observer_) {
    return;
  }
  observer_->OnLiveTranslateEnabledChanged();
}

}  // namespace ash::babelorca
