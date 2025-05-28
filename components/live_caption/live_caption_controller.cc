// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "media/base/media_switches.h"

namespace captions {

LiveCaptionController::LiveCaptionController(
    PrefService* profile_prefs,
    PrefService* global_prefs,
    const std::string& application_locale,
    content::BrowserContext* browser_context,
    std::unique_ptr<CaptionControllerBase::Delegate> delegate)
    : CaptionControllerBase(profile_prefs,
                            application_locale,
                            std::move(delegate)),
      global_prefs_(global_prefs),
      browser_context_(browser_context),
      caption_bubble_settings_(
          std::make_unique<LiveCaptionBubbleSettings>(profile_prefs)) {
  base::UmaHistogramBoolean("Accessibility.LiveCaption.FeatureEnabled2",
                            IsLiveCaptionFeatureSupported());

  // Hidden behind a feature flag.
  if (!IsLiveCaptionFeatureSupported()) {
    return;
  }
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kEnableLiveCaptionPrefForTesting)) {
    profile_prefs->SetBoolean(prefs::kLiveCaptionEnabled, true);
  }

  pref_change_registrar()->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&LiveCaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar()->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&LiveCaptionController::OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));

  enabled_ = IsLiveCaptionEnabled();
  base::UmaHistogramBoolean("Accessibility.LiveCaption2", enabled_);
  MaybeSetLiveCaptionLanguage();

  if (enabled_) {
    StartLiveCaption();
  }
}

LiveCaptionController::~LiveCaptionController() {
  if (enabled_) {
    enabled_ = false;
    StopLiveCaption();
  }
}

// static
void LiveCaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionBubbleExpanded, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionMaskOffensiveWords, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Initially default the language to en-US. The language
  // preference value will be set to a default language when Live Caption is
  // enabled for the first time.
  registry->RegisterStringPref(prefs::kLiveCaptionLanguageCode,
                               speech::kUsEnglishLocale,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterListPref(
      prefs::kLiveCaptionMediaFoundationRendererErrorSilenced,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void LiveCaptionController::OnLiveCaptionEnabledChanged() {
  bool enabled = IsLiveCaptionEnabled();
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;

  if (enabled) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
    speech::SodaInstaller::GetInstance()->SetUninstallTimer(profile_prefs(),
                                                            global_prefs_);
  }
}

void LiveCaptionController::OnLiveCaptionLanguageChanged() {
  if (enabled_) {
    const auto language_code = GetLanguageCode();
    auto* soda_installer = speech::SodaInstaller::GetInstance();
    // Only trigger an install when the language is not already installed.
    if (!soda_installer->IsSodaInstalled(
            speech::GetLanguageCode(language_code))) {
      soda_installer->InstallLanguage(language_code, global_prefs_);
    }
  }
}

bool LiveCaptionController::IsLiveCaptionEnabled() {
  return profile_prefs()->GetBoolean(prefs::kLiveCaptionEnabled);
}

void LiveCaptionController::StartLiveCaption() {
  DCHECK(enabled_);
  // The SodaInstaller determines whether SODA is already on the device and
  // whether or not to download. Once SODA is on the device and ready, the
  // SODAInstaller calls OnSodaInstalled on its observers. The UI is created at
  // that time.
  if (speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(GetLanguageCode()))) {
    CreateUI();
  } else {
    speech::SodaInstaller::GetInstance()->AddObserver(this);
    speech::SodaInstaller::GetInstance()->Init(profile_prefs(), global_prefs_);
  }
}

void LiveCaptionController::StopLiveCaption() {
  DCHECK(!enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  DestroyUI();
}

CaptionBubbleSettings* LiveCaptionController::caption_bubble_settings() {
  return caption_bubble_settings_.get();
}

void LiveCaptionController::OnSodaInstalled(
    speech::LanguageCode language_code) {
  // Live Caption should always be enabled when this is called. If Live Caption
  // has been disabled, then this should not be observing the SodaInstaller
  // anymore.
  DCHECK(enabled_);
  bool is_language_code_for_live_caption =
      prefs::IsLanguageCodeForLiveCaption(language_code, profile_prefs());

  if (is_language_code_for_live_caption) {
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
    CreateUI();
  }
}

void LiveCaptionController::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  // Check that language code matches the selected language for Live Caption or
  // is LanguageCode::kNone (signifying the SODA binary failed).
  if (!prefs::IsLanguageCodeForLiveCaption(language_code, profile_prefs()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    profile_prefs()->SetBoolean(prefs::kLiveCaptionEnabled, false);
  }
}

const std::string LiveCaptionController::GetLanguageCode() const {
  return prefs::GetLiveCaptionLanguageCode(profile_prefs());
}

bool LiveCaptionController::DispatchTranscription(
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_controller()) {
    return false;
  }
  return caption_bubble_controller()->OnTranscription(caption_bubble_context,
                                                      result);
}

void LiveCaptionController::OnError(
    CaptionBubbleContext* caption_bubble_context,
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback error_clicked_callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  if (!caption_bubble_controller()) {
    CreateUI();
  }
  caption_bubble_controller()->OnError(caption_bubble_context, error_type,
                                       std::move(error_clicked_callback),
                                       std::move(error_silenced_callback));
}

void LiveCaptionController::OnAudioStreamEnd(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_controller()) {
    return;
  }
  caption_bubble_controller()->OnAudioStreamEnd(caption_bubble_context);
}

void LiveCaptionController::OnLanguageIdentificationEvent(
    CaptionBubbleContext* caption_bubble_context,
    const media::mojom::LanguageIdentificationEventPtr& event) {
  // TODO(crbug.com/40167928): Implement the UI for language identification.
  if (caption_bubble_controller()) {
    return caption_bubble_controller()->OnLanguageIdentificationEvent(
        caption_bubble_context, event);
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
void LiveCaptionController::OnToggleFullscreen(
    CaptionBubbleContext* caption_bubble_context) {
  if (!enabled_) {
    return;
  }
  // The easiest way to move the Live Caption UI to the right workspace is to
  // simply destroy and recreate the UI. The UI will automatically be created
  // in the workspace of the browser window that is transmitting captions.
  DestroyUI();
  CreateUI();
}
#endif

void LiveCaptionController::MaybeSetLiveCaptionLanguage() {
  // If the current Live Caption language is not installed,
  // reset the Live Caption language code to the application locale or preferred
  // language if available.
  if (speech::SodaInstaller::GetInstance() &&
      profile_prefs()->GetString(prefs::kLiveCaptionLanguageCode) ==
          speech::kUsEnglishLocale &&
      speech::SodaInstaller::GetInstance()
          ->GetLanguagePath(
              profile_prefs()->GetString(prefs::kLiveCaptionLanguageCode))
          .empty()) {
    speech::SodaInstaller::GetInstance()->UnregisterLanguage(
        speech::kUsEnglishLocale, global_prefs_);
    speech::SodaInstaller::GetInstance()->RegisterLanguage(
        speech::GetDefaultLiveCaptionLanguage(application_locale(),
                                              profile_prefs()),
        global_prefs_);
    profile_prefs()->SetString(prefs::kLiveCaptionLanguageCode,
                               speech::GetDefaultLiveCaptionLanguage(
                                   application_locale(), profile_prefs()));
  }
}

}  // namespace captions
