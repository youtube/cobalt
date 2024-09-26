// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_delegate.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_util.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace {

const char kDeclineTranslate[] = "Translate.DeclineTranslate";
const char kRevertTranslation[] = "Translate.RevertTranslation";
const char kPerformTranslate[] = "Translate.Translate";
const char kPerformTranslateAmpCacheUrl[] = "Translate.Translate.AMPCacheURL";
const char kNeverTranslateLang[] = "Translate.NeverTranslateLang";
const char kNeverTranslateSite[] = "Translate.NeverTranslateSite";
const char kAlwaysTranslateLang[] = "Translate.AlwaysTranslateLang";
const char kModifySourceLang[] = "Translate.ModifyOriginalLang";
const char kModifyTargetLang[] = "Translate.ModifyTargetLang";
const char kShowErrorUI[] = "Translate.ShowErrorUI";

// Returns whether |url| fits pattern of an AMP cache url.
// Note this is a copy of logic in amp_page_load_metrics_observer.cc
// TODO(crbug.com/1064974) Factor out into shared utility.
bool IsLikelyAmpCacheUrl(const GURL& url) {
  // Our heuristic to identify AMP cache URLs is to check for the presence of
  // the amp_js_v query param.
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "amp_js_v")
      return true;
  }
  return false;
}

}  // namespace

namespace translate {

TranslateUIDelegate::TranslateUIDelegate(
    const base::WeakPtr<TranslateManager>& translate_manager,
    const std::string& source_language,
    const std::string& target_language)
    : translate_driver_(
          translate_manager->translate_client()->GetTranslateDriver()),
      translate_manager_(translate_manager),
      translate_ui_languages_manager_(
          std::make_unique<TranslateUILanguagesManager>(translate_manager,
                                                        source_language,
                                                        target_language)),
      prefs_(translate_manager_->translate_client()->GetTranslatePrefs()) {
  DCHECK(translate_driver_);
  DCHECK(translate_manager_);
  DCHECK(translate_ui_languages_manager_);

  if (base::FeatureList::IsEnabled(
          language::kContentLanguagesInLanguagePicker)) {
    MaybeSetContentLanguages();

    if (!base::GetFieldTrialParamByFeatureAsBool(
            language::kContentLanguagesInLanguagePicker,
            language::kContentLanguagesDisableObserversParam,
            false /* default */)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      const std::string& pref_name = language::prefs::kPreferredLanguages;
#else
      const std::string& pref_name = language::prefs::kAcceptLanguages;
#endif

      // Also start listening for changes in the accept languages.
      PrefService* pref_service =
          translate_manager->translate_client()->GetPrefs();
      pref_change_registrar_.Init(pref_service);
      pref_change_registrar_.Add(
          pref_name,
          base::BindRepeating(&TranslateUIDelegate::MaybeSetContentLanguages,
                              base::Unretained(this)));
    }
  }
}

TranslateUIDelegate::~TranslateUIDelegate() = default;

void TranslateUIDelegate::UpdateAndRecordSourceLanguageIndex(
    size_t language_index) {
  if (!translate_ui_languages_manager_->UpdateSourceLanguageIndex(
          language_index)) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN(kModifySourceLang, true);

  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogSourceLanguage(
        translate_ui_languages_manager_->GetLanguageCodeAt(language_index));
  }
}

void TranslateUIDelegate::UpdateAndRecordSourceLanguage(
    const std::string& language_code) {
  if (!translate_ui_languages_manager_->UpdateSourceLanguage(language_code)) {
    return;
  }

  if (translate_manager_) {
    translate_manager_->mutable_translate_event()->set_modified_source_language(
        language_code);
  }
}

void TranslateUIDelegate::UpdateAndRecordTargetLanguageIndex(
    size_t language_index) {
  if (!translate_ui_languages_manager_->UpdateTargetLanguageIndex(
          language_index)) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN(kModifyTargetLang, true);

  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogTargetLanguage(
        translate_ui_languages_manager_->GetLanguageCodeAt(language_index),
        TranslateBrowserMetrics::TargetLanguageOrigin::kChangedByUser);
  }
}

void TranslateUIDelegate::UpdateAndRecordTargetLanguage(
    const std::string& language_code) {
  if (!translate_ui_languages_manager_->UpdateTargetLanguage(language_code)) {
    return;
  }

  if (translate_manager_) {
    translate_manager_->mutable_translate_event()->set_modified_target_language(
        language_code);
  }
}

void TranslateUIDelegate::OnErrorShown(TranslateErrors error_type) {
  DCHECK_LE(TranslateErrors::NONE, error_type);
  DCHECK_LT(error_type, TranslateErrors::TRANSLATE_ERROR_MAX);

  if (error_type == TranslateErrors::NONE) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kShowErrorUI, error_type,
                            TranslateErrors::TRANSLATE_ERROR_MAX);
}

const LanguageState* TranslateUIDelegate::GetLanguageState() {
  if (translate_manager_) {
    return translate_manager_->GetLanguageState();
  }
  return nullptr;
}

void TranslateUIDelegate::GetContentLanguagesCodes(
    std::vector<std::string>* content_codes) const {
  DCHECK(content_codes != nullptr);
  content_codes->clear();

  for (auto& entry : translatable_content_languages_codes_) {
    content_codes->push_back(entry);
  }
}

void TranslateUIDelegate::Translate() {
  if (!translate_driver_->IsIncognito()) {
    prefs_->ResetTranslationDeniedCount(
        translate_ui_languages_manager_->GetSourceLanguageCode());
    prefs_->ResetTranslationIgnoredCount(
        translate_ui_languages_manager_->GetSourceLanguageCode());
    prefs_->IncrementTranslationAcceptedCount(
        translate_ui_languages_manager_->GetSourceLanguageCode());
  }

  prefs_->SetRecentTargetLanguage(
      translate_ui_languages_manager_->GetTargetLanguageCode());

  if (translate_manager_) {
    translate_manager_->RecordTranslateEvent(
        metrics::TranslateEventProto::USER_ACCEPT);
    translate_manager_->TranslatePage(
        translate_ui_languages_manager_->GetSourceLanguageCode(),
        translate_ui_languages_manager_->GetTargetLanguageCode(), false,
        translate_manager_->GetActiveTranslateMetricsLogger()
            ->GetNextManualTranslationType(
                /*is_context_menu_initiated_translation=*/false));
    UMA_HISTOGRAM_BOOLEAN(kPerformTranslate, true);
    if (IsLikelyAmpCacheUrl(translate_driver_->GetLastCommittedURL()))
      UMA_HISTOGRAM_BOOLEAN(kPerformTranslateAmpCacheUrl, true);
  }
}

void TranslateUIDelegate::RevertTranslation() {
  if (translate_manager_ &&
      translate_manager_->GetLanguageState()->IsPageTranslated()) {
    translate_manager_->RevertTranslation();
    UMA_HISTOGRAM_BOOLEAN(kRevertTranslation, true);
  }
}

void TranslateUIDelegate::TranslationDeclined(bool explicitly_closed) {
  if (!translate_driver_->IsIncognito()) {
    const std::string& language =
        translate_ui_languages_manager_->GetSourceLanguageCode();
    if (explicitly_closed) {
      prefs_->ResetTranslationAcceptedCount(language);
      prefs_->IncrementTranslationDeniedCount(language);
    } else {
      prefs_->IncrementTranslationIgnoredCount(language);
    }
  }

  // Remember that the user declined the translation so as to prevent showing a
  // translate UI for that page again.  (TranslateManager initiates translations
  // when getting a LANGUAGE_DETERMINED from the page, which happens when a load
  // stops. That could happen multiple times, including after the user already
  // declined the translation.)
  if (translate_manager_) {
    translate_manager_->RecordTranslateEvent(
        explicitly_closed ? metrics::TranslateEventProto::USER_DECLINE
                          : metrics::TranslateEventProto::USER_IGNORE);
    if (explicitly_closed)
      translate_manager_->GetLanguageState()->set_translation_declined(true);
  }

  if (explicitly_closed) {
    UMA_HISTOGRAM_BOOLEAN(kDeclineTranslate, true);
  }
}

bool TranslateUIDelegate::IsLanguageBlocked() const {
  return prefs_->IsBlockedLanguage(
      translate_ui_languages_manager_->GetSourceLanguageCode());
}

void TranslateUIDelegate::SetLanguageBlocked(bool value) {
  if (value) {
    prefs_->AddToLanguageList(
        translate_ui_languages_manager_->GetSourceLanguageCode(),
        /*force_blocked=*/true);
    if (translate_manager_) {
      // Translation has been blocked for this language. Capture that in the
      // metrics. Note that we don't capture a language being unblocked... which
      // is not the same as accepting a given translation for this language.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_LANGUAGE);
    }
  } else {
    prefs_->UnblockLanguage(
        translate_ui_languages_manager_->GetSourceLanguageCode());
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateLang, value);
}

bool TranslateUIDelegate::IsSiteOnNeverPromptList() const {
  std::string host = GetPageHost();
  return !host.empty() && prefs_->IsSiteOnNeverPromptList(host);
}

bool TranslateUIDelegate::CanAddSiteToNeverPromptList() const {
  return !GetPageHost().empty();
}

void TranslateUIDelegate::SetNeverPromptSite(bool value) {
  std::string host = GetPageHost();
  if (host.empty())
    return;

  if (value) {
    prefs_->AddSiteToNeverPromptList(host);
    if (translate_manager_) {
      // Translation has been blocked for this site. Capture that in the metrics
      // Note that we don't capture a language being unblocked... which is not
      // the same as accepting a given translation for this site.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_SITE);
    }
  } else {
    prefs_->RemoveSiteFromNeverPromptList(host);
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateSite, value);
}

bool TranslateUIDelegate::ShouldAlwaysTranslate() const {
  return prefs_->IsLanguagePairOnAlwaysTranslateList(
      translate_ui_languages_manager_->GetSourceLanguageCode(),
      translate_ui_languages_manager_->GetTargetLanguageCode());
}

void TranslateUIDelegate::SetAlwaysTranslate(bool value) {
  const std::string& source_lang =
      translate_ui_languages_manager_->GetSourceLanguageCode();
  const std::string& target_lang =
      translate_ui_languages_manager_->GetTargetLanguageCode();
  if (value) {
    prefs_->AddLanguagePairToAlwaysTranslateList(source_lang, target_lang);
    // A default translation mapping has been accepted for this language.
    // Capture that in the metrics. Note that we don't capture a language being
    // unmapped... which is not the same as accepting some other translation
    // for this language.
    if (translate_manager_) {
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_ALWAYS_TRANSLATE_LANGUAGE);
    }
    // If a language is being added to the always translate list on a
    // blocklisted site, remove that site from the blocklist.
    if (IsSiteOnNeverPromptList())
      SetNeverPromptSite(false);
  } else {
    prefs_->RemoveLanguagePairFromAlwaysTranslateList(source_lang, target_lang);
  }

  UMA_HISTOGRAM_BOOLEAN(kAlwaysTranslateLang, value);
}

bool TranslateUIDelegate::ShouldAlwaysTranslateBeCheckedByDefault() const {
  return ShouldAlwaysTranslate();
}

bool TranslateUIDelegate::ShouldShowAlwaysTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationAcceptedCount(
             translate_ui_languages_manager_->GetSourceLanguageCode()) >=
             kAlwaysTranslateShortcutMinimumAccepts;
}

bool TranslateUIDelegate::ShouldShowNeverTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationDeniedCount(
             translate_ui_languages_manager_->GetSourceLanguageCode()) >=
             kNeverTranslateShortcutMinimumDenials;
}

void TranslateUIDelegate::OnUIClosedByUser() {
  if (translate_manager_)
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(false);
}

void TranslateUIDelegate::ReportUIInteraction(UIInteraction ui_interaction) {
  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIInteraction(
        ui_interaction);
  }
}

void TranslateUIDelegate::ReportUIChange(bool is_ui_shown) {
  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(
        is_ui_shown);
  }
}

void TranslateUIDelegate::MaybeSetContentLanguages() {
  std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();
  translatable_content_languages_codes_.clear();
  prefs_->GetTranslatableContentLanguages(
      locale, &translatable_content_languages_codes_);
}

bool TranslateUIDelegate::IsIncognito() const {
  if (!translate_manager_)
    return false;
  TranslateClient* client = translate_manager_->translate_client();
  TranslateDriver* driver = client->GetTranslateDriver();
  return driver ? driver->IsIncognito() : false;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

bool TranslateUIDelegate::ShouldAutoAlwaysTranslate() {
  // Don't trigger if it's off the record or already set to always translate.
  if (IsIncognito() || ShouldAlwaysTranslate())
    return false;

  const std::string& source_language =
      translate_ui_languages_manager_->GetSourceLanguageCode();
  // Don't trigger for unknown source language.
  if (source_language == kUnknownLanguageCode)
    return false;

  bool always_translate =
      (prefs_->GetTranslationAcceptedCount(source_language) >=
           GetAutoAlwaysThreshold() &&
       prefs_->GetTranslationAutoAlwaysCount(source_language) <
           GetMaximumNumberOfAutoAlways());

  if (always_translate) {
    // Auto-always will be triggered. Need to increment the auto-always
    // counter.
    prefs_->IncrementTranslationAutoAlwaysCount(source_language);
    // Reset translateAcceptedCount so that auto-always could be triggered
    // again.
    prefs_->ResetTranslationAcceptedCount(source_language);
  }
  return always_translate;
}

bool TranslateUIDelegate::ShouldAutoNeverTranslate() {
  if (IsIncognito())
    return false;

  const std::string& source_language =
      translate_ui_languages_manager_->GetSourceLanguageCode();
  // Don't trigger if this language is already blocked.
  if (!prefs_->CanTranslateLanguage(source_language))
    return false;

  int auto_never_count = prefs_->GetTranslationAutoNeverCount(source_language);

  // At the beginning (auto_never_count == 0), deniedCount starts at 0 and is
  // off-by-one (because this checking is done before increment). However,
  // after auto-never is triggered once (auto_never_count > 0), deniedCount
  // starts at 1. So there is no off-by-one by then.
  int off_by_one = auto_never_count == 0 ? 1 : 0;

  bool never_translate =
      (prefs_->GetTranslationDeniedCount(source_language) + off_by_one >=
           GetAutoNeverThreshold() &&
       auto_never_count < GetMaximumNumberOfAutoNever());
  if (never_translate) {
    // Auto-never will be triggered. Need to increment the auto-never counter.
    prefs_->IncrementTranslationAutoNeverCount(source_language);
    // Reset translateDeniedCount so that auto-never could be triggered again.
    prefs_->ResetTranslationDeniedCount(source_language);
  }
  return never_translate;
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

std::string TranslateUIDelegate::GetPageHost() const {
  if (!translate_driver_->HasCurrentPage()) {
    return std::string();
  }
  return translate_driver_->GetLastCommittedURL().HostNoBrackets();
}

}  // namespace translate
