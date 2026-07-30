// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace quick_answers {

// Extracts quick answer result out of the search response. Note that the
// returned `quick_answers_session` may be `nullptr`.
std::unique_ptr<QuickAnswersSession> ParseSearchResponse(
    const std::string& response_body);

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_SEARCH_RESPONSE_PARSER_H_
