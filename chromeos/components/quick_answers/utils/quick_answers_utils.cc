// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

#include <string>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

namespace {

constexpr char kUnitConversionQueryRewriteTemplate[] = "Convert:%s";
constexpr char kDictionaryQueryRewriteTemplate[] = "%s %s";
constexpr char kTranslationQueryRewriteTemplate[] = "Translate:%s";

constexpr char kDictionaryQueryKeyword[] = "Define";
constexpr char kDictionaryQueryKeywordES[] = "Definir";
constexpr char kDictionaryQueryKeywordIT[] = "Definire";
constexpr char kDictionaryQueryKeywordFR[] = "Définir";
constexpr char kDictionaryQueryKeywordPT[] = "Definir";
constexpr char kDictionaryQueryKeywordDE[] = "Definieren";

}  // namespace

// Get the best dictionary query keyword based on the language.
const char* GetDictionaryQueryKeyword(const std::string& language) {
  if (language == "es")
    return kDictionaryQueryKeywordES;
  if (language == "it")
    return kDictionaryQueryKeywordIT;
  if (language == "fr")
    return kDictionaryQueryKeywordFR;
  if (language == "pt")
    return kDictionaryQueryKeywordPT;
  if (language == "de")
    return kDictionaryQueryKeywordDE;

  return kDictionaryQueryKeyword;
}

const PreprocessedOutput PreprocessRequest(const IntentInfo& intent_info) {
  PreprocessedOutput processed_output;
  processed_output.intent_info = intent_info;
  processed_output.query = intent_info.intent_text;

  switch (intent_info.intent_type) {
    case IntentType::kUnit:
      processed_output.query = base::StringPrintf(
          kUnitConversionQueryRewriteTemplate, intent_info.intent_text.c_str());
      break;
    case IntentType::kDictionary:
      processed_output.query = base::StringPrintf(
          kDictionaryQueryRewriteTemplate,
          GetDictionaryQueryKeyword(intent_info.source_language),
          intent_info.intent_text.c_str());
      break;
    case IntentType::kTranslation:
      processed_output.query = base::StringPrintf(
          kTranslationQueryRewriteTemplate, intent_info.intent_text.c_str());
      break;
    case IntentType::kUnknown:
      // TODO(llin): Update to NOTREACHED after integrating with TCLib.
      break;
  }
  return processed_output;
}

std::string BuildDefinitionTitleText(const std::string& query_term,
                                     const std::string& phonetics) {
  return l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_DEFINITION_TITLE_TEXT,
                                   base::UTF8ToUTF16(query_term),
                                   base::UTF8ToUTF16(phonetics));
}

std::string BuildKpEntityTitleText(const std::string& average_score,
                                   const std::string& aggregated_count) {
  return l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_RATING_REVIEW_TITLE_TEXT,
                                   base::UTF8ToUTF16(average_score),
                                   base::UTF8ToUTF16(aggregated_count));
}

std::string BuildTranslationTitleText(const IntentInfo& intent_info) {
  auto locale_name = l10n_util::GetDisplayNameForLocale(
      intent_info.source_language, intent_info.device_language, true);
  return l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_TRANSLATION_TITLE_TEXT,
                                   base::UTF8ToUTF16(intent_info.intent_text),
                                   locale_name);
}

std::string BuildTranslationTitleText(const std::string& query_text,
                                      const std::string& locale_name) {
  return l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_TRANSLATION_TITLE_TEXT,
                                   base::UTF8ToUTF16(query_text),
                                   base::UTF8ToUTF16(locale_name));
}

std::string BuildUnitConversionResultText(const std::string& result_value,
                                          const std::string& name) {
  return l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_UNIT_CONVERSION_RESULT_TEXT,
      base::UTF8ToUTF16(result_value), base::UTF8ToUTF16(name));
}

std::string UnescapeStringForHTML(const std::string& string) {
  return base::UTF16ToUTF8(base::UnescapeForHTML(base::UTF8ToUTF16(string)));
}

absl::optional<double> GetRatio(const double value1, const double value2) {
  if (value1 == 0 || value2 == 0)
    return absl::nullopt;

  return std::max(value1, value2) / std::min(value1, value2);
}

}  // namespace quick_answers
