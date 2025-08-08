// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"

#include <memory>
#include <string_view>
#include <tuple>

#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/form_data_android_bridge.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

FormDataAndroid::FormDataAndroid(const FormData& form)
    : form_(form),
      bridge_(AndroidAutofillBridgeFactory::GetInstance()
                  .CreateFormDataAndroidBridge()) {
  fields_.reserve(form_.fields.size());
  for (FormFieldData& field : form_.fields) {
    fields_.push_back(std::make_unique<FormFieldDataAndroid>(&field));
  }
}

FormDataAndroid::~FormDataAndroid() = default;

base::android::ScopedJavaLocalRef<jobject> FormDataAndroid::GetJavaPeer() {
  return bridge_->GetOrCreateJavaPeer(form_, fields_);
}

void FormDataAndroid::UpdateFromJava() {
  for (std::unique_ptr<FormFieldDataAndroid>& field : fields_)
    field->UpdateFromJava();
}

void FormDataAndroid::OnFormFieldDidChange(size_t index,
                                           std::u16string_view value) {
  fields_[index]->OnFormFieldDidChange(value);
}

bool FormDataAndroid::GetFieldIndex(const FormFieldData& field, size_t* index) {
  for (size_t i = 0; i < form_.fields.size(); ++i) {
    if (form_.fields[i].SameFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::GetSimilarFieldIndex(const FormFieldData& field,
                                           size_t* index) {
  for (size_t i = 0; i < form_.fields.size(); ++i) {
    if (fields_[i]->SimilarFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::SimilarFormAs(const FormData& form) const {
  // Note that comparing unique renderer ids alone is not a strict enough check,
  // since these remain constant even if the page has dynamically modified its
  // fields to have different labels, form control types, etc.
  auto SimilarityTuple = [](const FormData& f) {
    return std::tuple_cat(std::tie(f.host_frame, f.unique_renderer_id, f.name,
                                   f.id_attribute, f.name_attribute, f.url,
                                   f.action, f.is_action_empty, f.is_form_tag),
                          std::make_tuple(f.fields.size()));
  };

  if (SimilarityTuple(form_) != SimilarityTuple(form)) {
    return false;
  }
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (!fields_[i]->SimilarFieldAs(form.fields[i])) {
      return false;
    }
  }
  return true;
}

void FormDataAndroid::UpdateFieldTypes(const FormStructure& form_structure) {
  // This form has been changed after the query starts. Ignore this response,
  // a new one is on the way.
  if (form_structure.field_count() != fields_.size())
    return;
  auto form_field_data_android = fields_.begin();
  for (const auto& autofill_field : form_structure) {
    DCHECK(form_field_data_android->get()->SimilarFieldAs(*autofill_field));
    std::vector<AutofillType> server_predictions;
    for (const auto& prediction : autofill_field->server_predictions()) {
      server_predictions.emplace_back(
          static_cast<ServerFieldType>(prediction.type()));
    }
    form_field_data_android->get()->UpdateAutofillTypes(
        FormFieldDataAndroid::FieldTypes(
            AutofillType(autofill_field->heuristic_type()),
            AutofillType(autofill_field->server_type()),
            autofill_field->ComputedType(), std::move(server_predictions)));
    if (++form_field_data_android == fields_.end())
      break;
  }
}

std::vector<int> FormDataAndroid::UpdateFieldVisibilities(
    const FormData& form) {
  CHECK_EQ(form_.fields.size(), form.fields.size());
  CHECK_EQ(form_.fields.size(), fields_.size());

  // We rarely expect to find any difference in visibility - therefore do not
  // reserve space in the vector.
  std::vector<int> indices;
  for (size_t i = 0; i < form_.fields.size(); ++i) {
    if (form_.fields[i].IsFocusable() != form.fields[i].IsFocusable()) {
      fields_[i]->OnFormFieldVisibilityDidChange(form.fields[i]);
      indices.push_back(i);
    }
  }
  return indices;
}

}  // namespace autofill
