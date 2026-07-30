// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

namespace quick_answers {

std::unique_ptr<TranslationResult> ParseTranslationResponse(
    const std::string& response_body) {
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(response_body,
                                                    base::JSON_PARSE_RFC);
  if (!result.has_value()) {
    LOG(ERROR) << "JSON parsing failed: " << result.error().message;
    return nullptr;
  }

  const base::ListValue* translations =
      result->GetDict().FindListByDottedPath("data.translations");
  if (!translations) {
    LOG(ERROR) << "Can't find translations result list.";
    return nullptr;
  }

  DCHECK_EQ(translations->size(), 1ul);

  const std::string* translated_text_ptr =
      translations->front().GetDict().FindString("translatedText");
  if (!translated_text_ptr) {
    LOG(ERROR) << "Can't find a translated text.";
    return nullptr;
  }
  std::unique_ptr<TranslationResult> translation_result =
      std::make_unique<TranslationResult>();
  translation_result->translated_text =
      UnescapeStringForHTML(*translated_text_ptr);
  return translation_result;
}

}  // namespace quick_answers
