// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_ANNOTATION_PROMPT_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_ANNOTATION_PROMPT_STRIKE_DATABASE_H_

#include <set>
#include <string>

#include "components/autofill/core/browser/strike_databases/history_clearable_strike_database.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill_ai {

struct AutofillAiAnnotationPromptStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AutofillAiAnnotationPrompt";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;

  static std::string GetId(autofill::FormSignature signature) {
    return base::NumberToString(signature.value());
  }
};

// Records the number of times a user declines an annotion prompt for a specific
// form signature.
using AutofillPrectionImprovementsAnnotationPromptStrikeDatabase =
    autofill::HistoryClearableStrikeDatabase<
        AutofillAiAnnotationPromptStrikeDatabaseTraits>;

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_ANNOTATION_PROMPT_STRIKE_DATABASE_H_
