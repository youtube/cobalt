// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_client.h"
#include "ash/quick_insert/search/quick_insert_search_aggregator.h"
#include "ash/quick_insert/search/quick_insert_search_request.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace ash {
namespace {

constexpr int kMaxEmojiResults = 3;
constexpr int kMaxSymbolResults = 2;
constexpr int kMaxEmoticonResults = 2;

// These are taken from manifest files in:
// https://source.chromium.org/chromium/chromium/src/+/2be4329930cac782779c5083389b83e09a8bcb47:chrome/browser/resources/chromeos/input_method/
constexpr auto kImeToLangCode =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"xkb:us::eng", "en"},           {"xkb:us:intl:eng", "en"},
         {"xkb:us:altgr-intl:eng", "en"}, {"xkb:us:dvorak:eng", "en"},
         {"xkb:us:dvp:eng", "en"},        {"xkb:us:colemak:eng", "en"},
         {"xkb:us:workman:eng", "en"},    {"xkb:us:workman-intl:eng", "en"},
         {"xkb:fr::fra", "fr"},           {"xkb:be::fra", "fr"},
         {"xkb:ca::fra", "fr"},           {"xkb:ch:fr:fra", "fr"},
         {"xkb:ca:multix:fra", "fr"},     {"xkb:de::ger", "de"},
         {"xkb:de:neo:ger", "de"},        {"xkb:be::ger", "de"},
         {"xkb:ch::ger", "de"},           {"xkb:jp::jpn", "ja"},
         {"xkb:ca:eng:eng", "en"},        {"xkb:es::spa", "es"},
         {"xkb:dk::dan", "da"},           {"xkb:latam::spa", "es"},
         {"xkb:gb:extd:eng", "en"},       {"xkb:gb:dvorak:eng", "en"},
         {"xkb:fi::fin", "fi"},           {"xkb:no::nob", "no"},
         {"xkb:se::swe", "sv"},           {"nacl_mozc_us", "ja"},
         {"nacl_mozc_jp", "ja"}});

base::span<const emoji::EmojiSearchEntry> FirstNOrFewerElements(
    base::span<const emoji::EmojiSearchEntry> container,
    size_t n) {
  return container.first(std::min(container.size(), n));
}

const base::Value::Dict* LoadEmojiVariantsFromPrefs(PrefService* prefs) {
  if (prefs == nullptr) {
    return nullptr;
  }
  return prefs->GetDict(prefs::kEmojiPickerPreferences)
      .FindDict("preferred_variants");
}

std::vector<std::string> GetLanguageCodesFromPrefs(PrefService* prefs) {
  if (prefs == nullptr) {
    return {"en"};
  }

  // These codes should be in this order.
  // - First: Current active IME
  // - Remaining: All other IMEs that are enabled
  // There should be no duplicates.
  std::vector<std::string> results;

  // Get the currently active IME
  std::string current_ime = extension_ime_util::GetComponentIDByInputMethodID(
      prefs->GetString(prefs::kLanguageCurrentInputMethod));
  if (const auto& it = kImeToLangCode.find(current_ime);
      it != kImeToLangCode.end()) {
    results.push_back(std::string(it->second));
  }

  // Add ui language as second set of results.
  // EmojiSearch expects the "base language" without region e.g. "en" instead of
  // "en-US".
  std::string ui_locale(language::ExtractBaseLanguage(
      prefs->GetString(language::prefs::kApplicationLocale)));

  results.push_back(ui_locale);

  // All enabled engines
  std::vector<std::string> full_ids =
      base::SplitString(prefs->GetString(prefs::kLanguagePreloadEngines), ",",
                        base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);

  for (const std::string& id : full_ids) {
    std::string short_ime_id =
        extension_ime_util::GetComponentIDByInputMethodID(id);
    if (const auto& it = kImeToLangCode.find(short_ime_id);
        it != kImeToLangCode.end() && !base::Contains(results, it->second)) {
      results.push_back(std::string(it->second));
    }
  }

  if (results.empty()) {
    // If no languages supported then attempt to use English.
    results.push_back("en");
  }

  return results;
}

}  // namespace

QuickInsertSearchController::QuickInsertSearchController(
    base::TimeDelta burn_in_period)
    : burn_in_period_(burn_in_period) {}

QuickInsertSearchController::~QuickInsertSearchController() = default;

void QuickInsertSearchController::LoadEmojiLanguagesFromPrefs(
    PrefService* prefs) {
  pref_change_registrar_.Reset();
  if (prefs == nullptr) {
    return;
  }
  emoji_search_.LoadEmojiLanguages(GetLanguageCodesFromPrefs(prefs));

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kLanguagePreloadEngines,
      base::BindRepeating(&QuickInsertSearchController::LoadEmojiLanguages,
                          weak_ptr_factory_.GetWeakPtr(),
                          pref_change_registrar_.prefs()));
}

void QuickInsertSearchController::LoadEmojiLanguages(PrefService* prefs) {
  if (prefs == nullptr) {
    return;
  }
  emoji_search_.LoadEmojiLanguages(GetLanguageCodesFromPrefs(prefs));
}

void QuickInsertSearchController::StartSearch(
    QuickInsertClient* client,
    std::u16string_view query,
    std::optional<QuickInsertCategory> category,
    base::span<const QuickInsertCategory> available_categories,
    bool caps_lock_state_to_search,
    bool search_case_transforms,
    QuickInsertViewDelegate::SearchResultsCallback callback) {
  StopSearch();
  aggregator_ = std::make_unique<QuickInsertSearchAggregator>(
      burn_in_period_, std::move(callback));

  // TODO: b/348067874 - Hook `done_closure` up to `aggregator_`.
  search_request_ = std::make_unique<QuickInsertSearchRequest>(
      query, std::move(category),
      base::BindRepeating(
          &QuickInsertSearchAggregator::HandleSearchSourceResults,
          aggregator_->GetWeakPtr()),
      base::BindOnce(&QuickInsertSearchAggregator::HandleNoMoreResults,
                     aggregator_->GetWeakPtr()),
      client, available_categories, caps_lock_state_to_search,
      search_case_transforms);
}

void QuickInsertSearchController::StopSearch() {
  // The search request must be reset first so it can let the aggregator know
  // that it has been interrupted.
  search_request_.reset();
  aggregator_.reset();
}

void QuickInsertSearchController::StartEmojiSearch(
    PrefService* prefs,
    std::u16string_view query,
    QuickInsertViewDelegate::EmojiSearchResultsCallback callback) {
  const base::TimeTicks search_start = base::TimeTicks::Now();

  emoji::EmojiSearchResult results = emoji_search_.SearchEmoji(
      query, GetLanguageCodesFromPrefs(prefs), kMaxEmojiResults,
      kMaxSymbolResults, kMaxEmoticonResults);

  base::TimeDelta elapsed = base::TimeTicks::Now() - search_start;
  base::UmaHistogramTimes("Ash.Picker.Search.EmojiProvider.QueryTime", elapsed);

  std::vector<QuickInsertEmojiResult> emoji_results;
  emoji_results.reserve(kMaxEmojiResults + kMaxSymbolResults +
                        kMaxEmoticonResults);

  const base::Value::Dict* emoji_variants = LoadEmojiVariantsFromPrefs(prefs);

  for (const emoji::EmojiSearchEntry& result :
       FirstNOrFewerElements(results.emojis, kMaxEmojiResults)) {
    std::string emoji_string = result.emoji_string;
    if (emoji_variants != nullptr) {
      const std::string* variant_string =
          emoji_variants->FindString(emoji_string);
      if (variant_string != nullptr) {
        emoji_string = *variant_string;
      }
    }
    emoji_results.push_back(QuickInsertEmojiResult::Emoji(
        base::UTF8ToUTF16(emoji_string),
        base::UTF8ToUTF16(emoji_search_.GetEmojiName(emoji_string, "en"))));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrFewerElements(results.symbols, kMaxSymbolResults)) {
    emoji_results.push_back(QuickInsertEmojiResult::Symbol(
        base::UTF8ToUTF16(result.emoji_string),
        base::UTF8ToUTF16(
            emoji_search_.GetEmojiName(result.emoji_string, "en"))));
  }
  for (const emoji::EmojiSearchEntry& result :
       FirstNOrFewerElements(results.emoticons, kMaxEmoticonResults)) {
    emoji_results.push_back(QuickInsertEmojiResult::Emoticon(
        base::UTF8ToUTF16(result.emoji_string),
        base::UTF8ToUTF16(
            emoji_search_.GetEmojiName(result.emoji_string, "en"))));
  }

  std::move(callback).Run(std::move(emoji_results));
}

std::string QuickInsertSearchController::GetEmojiName(std::string_view emoji) {
  return emoji_search_.GetEmojiName(emoji, "en");
}

}  // namespace ash
