// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"

#include <string>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/password_manager/core/browser/protos/list_passwords_result.pb.h"
#include "components/password_manager/core/browser/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"

using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

// Keys used to serialize and deserialize password form data.
constexpr char kActionKey[] = "action";
constexpr char kFieldsKey[] = "fields";
constexpr char kFormControlTypeKey[] = "form_control_type";
constexpr char kFormDataKey[] = "form_data";
constexpr char kNameKey[] = "name";
constexpr char kSkipZeroClickKey[] = "skip_zero_click";
constexpr char kUrlKey[] = "url";

base::Value::Dict SerializeSignatureRelevantMembersInFormData(
    const FormData& form_data) {
  base::Value::Dict serialized_data;
  // Stored FormData is used only for signature calculations, therefore only
  // members that are used for signature calculation are stored.
  serialized_data.Set(kNameKey, form_data.name);
  serialized_data.Set(kUrlKey, form_data.url.spec());
  serialized_data.Set(kActionKey, form_data.action.spec());

  base::Value::List serialized_fields;
  for (const auto& field : form_data.fields) {
    base::Value::Dict serialized_field;
    // Stored FormFieldData is used only for signature calculations, therefore
    // only members that are used for signature calculation are stored.
    serialized_field.Set(kNameKey, field.name);
    serialized_field.Set(kFormControlTypeKey, field.form_control_type);
    serialized_fields.Append(std::move(serialized_field));
  }
  serialized_data.Set(kFieldsKey, std::move(serialized_fields));
  return serialized_data;
}

std::string SerializeOpaqueLocalData(const PasswordForm& password_form) {
  base::Value::Dict local_data_json;
  local_data_json.Set(kSkipZeroClickKey, password_form.skip_zero_click);

  base::Value::Dict serialized_form_data =
      SerializeSignatureRelevantMembersInFormData(password_form.form_data);
  local_data_json.Set(kFormDataKey, std::move(serialized_form_data));

  std::string serialized_local_data;
  JSONStringValueSerializer serializer(&serialized_local_data);
  serializer.Serialize(local_data_json);
  return serialized_local_data;
}

absl::optional<FormData> DeserializeFormData(
    base::Value::Dict& serialized_data) {
  std::string* form_name = serialized_data.FindString(kNameKey);
  std::string* form_url = serialized_data.FindString(kUrlKey);
  std::string* form_action = serialized_data.FindString(kActionKey);
  base::Value::List* fields = serialized_data.FindList(kFieldsKey);
  if (!form_name || !form_url || !form_action || !fields) {
    return absl::nullopt;
  }
  FormData form_data;
  form_data.name = base::UTF8ToUTF16(*form_name);
  form_data.url = GURL(*form_url);
  form_data.action = GURL(*form_action);

  for (auto& serialized_field : *fields) {
    base::Value::Dict* serialized_field_dictionary =
        serialized_field.GetIfDict();
    if (!serialized_field_dictionary) {
      return absl::nullopt;
    }
    FormFieldData field;
    std::string* field_name = serialized_field_dictionary->FindString(kNameKey);
    std::string* field_type =
        serialized_field_dictionary->FindString(kFormControlTypeKey);
    if (!field_name || !field_type) {
      return absl::nullopt;
    }
    field.name = base::UTF8ToUTF16(*field_name);
    field.form_control_type = *field_type;
    form_data.fields.push_back(field);
  }
  return form_data;
}

void DeserializeOpaqueLocalData(const std::string& opaque_metadata,
                                PasswordForm& password_form) {
  JSONStringValueDeserializer json_deserializer(opaque_metadata);
  std::unique_ptr<base::Value> root(
      json_deserializer.Deserialize(nullptr, nullptr));
  if (!root.get() || !root->is_dict()) {
    return;
  }

  base::Value::Dict serialized_data(std::move(*root).TakeDict());
  auto skip_zero_click = serialized_data.FindBool(kSkipZeroClickKey);
  auto* serialized_form_data = serialized_data.FindDict(kFormDataKey);
  if (!skip_zero_click.has_value() || !serialized_form_data) {
    return;
  }
  absl::optional<FormData> form_data =
      DeserializeFormData(*serialized_form_data);
  if (!form_data.has_value()) {
    return;
  }
  password_form.skip_zero_click = *skip_zero_click;
  password_form.form_data = std::move(form_data.value());
}

}  // namespace

PasswordWithLocalData PasswordWithLocalDataFromPassword(
    const PasswordForm& password_form) {
  PasswordWithLocalData password_with_local_data;

  *password_with_local_data.mutable_password_specifics_data() =
      SpecificsDataFromPassword(password_form, /*base_password_data=*/{});

  auto* local_data = password_with_local_data.mutable_local_data();
  local_data->set_opaque_metadata(SerializeOpaqueLocalData(password_form));
  if (!password_form.previously_associated_sync_account_email.empty()) {
    local_data->set_previously_associated_sync_account_email(
        password_form.previously_associated_sync_account_email);
  }

  return password_with_local_data;
}

PasswordForm PasswordFromProtoWithLocalData(
    const PasswordWithLocalData& password) {
  PasswordForm form = PasswordFromSpecifics(password.password_specifics_data());
  form.previously_associated_sync_account_email =
      password.local_data().previously_associated_sync_account_email();
  DeserializeOpaqueLocalData(password.local_data().opaque_metadata(), form);
  return form;
}

std::vector<PasswordForm> PasswordVectorFromListResult(
    const ListPasswordsResult& list_result) {
  std::vector<PasswordForm> forms;
  for (const PasswordWithLocalData& password : list_result.password_data()) {
    forms.push_back(PasswordFromProtoWithLocalData(password));
  }
  return forms;
}

}  // namespace password_manager
