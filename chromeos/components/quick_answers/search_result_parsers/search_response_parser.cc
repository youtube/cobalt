// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace quick_answers {
namespace {

// String to prepend to JSON responses to prevent XSSI. See http://go/xssi.
constexpr char kJsonSafetyPrefix[] = ")]}'\n";

std::unique_ptr<QuickAnswersSession> ProcessResult(const base::Value& result);

}  // namespace

std::unique_ptr<QuickAnswersSession> ParseSearchResponse(
    const std::string& response_body) {
  if (response_body.length() < strlen(kJsonSafetyPrefix) ||
      response_body.substr(0, strlen(kJsonSafetyPrefix)) != kJsonSafetyPrefix) {
    LOG(ERROR) << "Invalid search response.";
    return nullptr;
  }

  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(
          response_body.substr(strlen(kJsonSafetyPrefix)),
          base::JSON_PARSE_RFC);
  if (!result.has_value()) {
    LOG(ERROR) << "JSON parsing failed: " << result.error().message;
    return nullptr;
  }

  // Get the first result.
  const base::ListValue* entries = result->GetDict().FindList("results");
  if (!entries) {
    return nullptr;
  }

  for (const auto& entry : *entries) {
    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        ProcessResult(entry);
    if (quick_answers_session) {
      return quick_answers_session;
    }
  }

  return nullptr;
}

namespace {

std::unique_ptr<QuickAnswersSession> ProcessResult(const base::Value& result) {
  const base::DictValue& dict = result.GetDict();
  auto one_namespace_type = dict.FindInt("oneNamespaceType");
  if (!one_namespace_type.has_value()) {
    // Can't find valid one namespace type from the response.
    LOG(ERROR) << "Can't find valid one namespace type from the response.";
    return nullptr;
  }

  std::unique_ptr<ResultParser> result_parser =
      ResultParserFactory::Create(one_namespace_type.value());
  if (!result_parser) {
    return nullptr;
  }

  if (result_parser->SupportsNewInterface()) {
    // Try to parse from StructuredResult, which supports Rich Answers.
    std::unique_ptr<StructuredResult> structured_result =
        result_parser->ParseInStructuredResult(dict);
    if (!structured_result) {
      return nullptr;
    }

    std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
    if (!result_parser->PopulateQuickAnswer(*structured_result,
                                            quick_answer.get())) {
      return nullptr;
    }

    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        std::make_unique<QuickAnswersSession>();
    quick_answers_session->structured_result = std::move(structured_result);
    quick_answers_session->quick_answer = std::move(quick_answer);
    return quick_answers_session;
  }

  // If a parser does not support `StructuredResult`, falls back to `Parse`
  // method. This is for a parser which has not migrated to the new interfaces
  // yet.
  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  if (!result_parser->Parse(dict, quick_answer.get())) {
    return nullptr;
  }

  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);
  return quick_answers_session;
}

}  // namespace

}  // namespace quick_answers
