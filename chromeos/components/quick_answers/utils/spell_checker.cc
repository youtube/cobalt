// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/spell_checker.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

SpellChecker::SpellChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  quick_answers_state_observation_.Observe(QuickAnswersState::Get());
}

SpellChecker::~SpellChecker() {
  spellcheck_languages_.clear();
}

void SpellChecker::CheckSpelling(const std::string& word,
                                 CheckSpellingCallback callback) {
  auto iterator = spellcheck_languages_.begin();
  if (iterator == spellcheck_languages_.end()) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator->get()->CheckSpelling(
      word,
      base::BindOnce(&SpellChecker::CollectResults, base::Unretained(this),
                     word, std::move(callback), iterator,
                     iterator->get()->language(), languages_list_version_));
}

void SpellChecker::OnSettingsEnabled(bool enabled) {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/false);
}

void SpellChecker::OnConsentStatusUpdated(prefs::ConsentStatus status) {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/false);
}

void SpellChecker::OnApplicationLocaleReady(const std::string& locale) {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/true);
}

void SpellChecker::OnPreferredLanguagesChanged(
    const std::string& preferred_languages) {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/true);
}

void SpellChecker::OnEligibilityChanged(bool eligible) {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/false);
}

void SpellChecker::OnPrefsInitialized() {
  CheckEligibilityAndUpdateLanguages(/*should_recreate_languages_list=*/false);
}

void SpellChecker::CheckEligibilityAndUpdateLanguages(
    bool should_recreate_languages_list) {
  // Still waiting for all of the states to be ready.
  if (!QuickAnswersState::Get()->prefs_initialized()) {
    return;
  }

  bool should_enable_to_show_consent =
      QuickAnswersState::Get()->consent_status() ==
      prefs::ConsentStatus::kUnknown;
  if (!QuickAnswersState::Get()->is_eligible() ||
      (!QuickAnswersState::Get()->settings_enabled() &&
       !should_enable_to_show_consent)) {
    spellcheck_languages_.clear();
    languages_list_version_++;
    return;
  }

  // Return if languages list is already initialized and no need to recreate.
  if (!spellcheck_languages_.empty() && !should_recreate_languages_list) {
    return;
  }

  // Add application language.
  std::set<std::string> languages;
  languages.insert(
      l10n_util::GetLanguage(QuickAnswersState::Get()->application_locale()));

  // Add preferred languages if supported.
  auto preferred_languages_list =
      base::SplitString(QuickAnswersState::Get()->preferred_languages(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& locale : preferred_languages_list) {
    auto language = l10n_util::GetLanguage(locale);
    if (QuickAnswersState::Get()->IsSupportedLanguage(language))
      languages.insert(language);
  }

  spellcheck_languages_.clear();
  languages_list_version_++;
  for (auto language : languages) {
    spellcheck_languages_.push_back(
        std::make_unique<SpellCheckLanguage>(url_loader_factory_));
    spellcheck_languages_.back()->Initialize(language);
  }
}

void SpellChecker::CollectResults(const std::string& word,
                                  CheckSpellingCallback callback,
                                  SpellCheckLanguageIterator iterator,
                                  const std::string& language,
                                  int languages_list_version,
                                  bool is_correct) {
  if (is_correct) {
    std::move(callback).Run(true, language);
    return;
  }

  // The languages list has been updated, return false for the current call.
  if (languages_list_version != languages_list_version_) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator++;
  if (iterator == spellcheck_languages_.end()) {
    std::move(callback).Run(false, std::string());
    return;
  }

  iterator->get()->CheckSpelling(
      word,
      base::BindOnce(&SpellChecker::CollectResults, base::Unretained(this),
                     word, std::move(callback), iterator,
                     iterator->get()->language(), languages_list_version));
}

}  // namespace quick_answers
