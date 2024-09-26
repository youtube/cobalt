// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_generation_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace password_generation {

PasswordGenerationUIData::PasswordGenerationUIData(
    const gfx::RectF& bounds,
    int max_length,
    const std::u16string& generation_element,
    const std::u16string& user_typed_password,
    FieldRendererId generation_element_id,
    bool is_generation_element_password_type,
    base::i18n::TextDirection text_direction,
    const FormData& form_data)
    : bounds(bounds),
      max_length(max_length),
      generation_element(generation_element),
      user_typed_password(user_typed_password),
      generation_element_id(generation_element_id),
      is_generation_element_password_type(is_generation_element_password_type),
      text_direction(text_direction),
      form_data(form_data) {}

PasswordGenerationUIData::PasswordGenerationUIData() = default;
PasswordGenerationUIData::PasswordGenerationUIData(
    const PasswordGenerationUIData& rhs) = default;
PasswordGenerationUIData::PasswordGenerationUIData(
    PasswordGenerationUIData&& rhs) = default;

PasswordGenerationUIData& PasswordGenerationUIData::operator=(
    const PasswordGenerationUIData& rhs) = default;
PasswordGenerationUIData& PasswordGenerationUIData::operator=(
    PasswordGenerationUIData&& rhs) = default;

PasswordGenerationUIData::~PasswordGenerationUIData() = default;

void LogPasswordGenerationEvent(PasswordGenerationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.Event", event,
                            EVENT_ENUM_COUNT);
}

}  // namespace password_generation
}  // namespace autofill
