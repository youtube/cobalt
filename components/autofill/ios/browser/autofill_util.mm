// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_util.h"
#import "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormControlType;
using base::NumberToString;
using base::StringToUint;

namespace {
// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

// Runs |callback| with the NSString value of |res|.
// |callback| must be non-null.
void ConvertValueToNSString(base::OnceCallback<void(NSString*)> callback,
                            const base::Value* res) {
  DCHECK(!callback.is_null());

  NSString* result = nil;
  if (res && res->is_string()) {
    result = base::SysUTF8ToNSString(res->GetString());
  }
  std::move(callback).Run(result);
}

// Runs |callback| with the BOOL value of |res|. |callback| must be non-null.
void ConvertValueToBool(base::OnceCallback<void(BOOL)> callback,
                        const base::Value* res) {
  DCHECK(!callback.is_null());

  BOOL result = NO;
  if (res && res->is_bool()) {
    result = res->GetBool();
  }
  std::move(callback).Run(result);
}

}  // namespace

namespace autofill {

bool IsContextSecureForWebState(web::WebState* web_state) {
  // This implementation differs slightly from other platforms. Other platforms'
  // implementations check for the presence of active mixed content, but because
  // the iOS web view blocks active mixed content without an option to run it,
  // there is no need to check for active mixed content here.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  const web::NavigationItem* nav_item = manager->GetLastCommittedItem();
  if (!nav_item) {
    return false;
  }

  const web::SSLStatus& ssl = nav_item->GetSSL();
  return nav_item->GetURL().SchemeIsCryptographic() && ssl.certificate &&
         !net::IsCertStatusError(ssl.cert_status);
}

std::unique_ptr<base::Value> ParseJson(NSString* json_string) {
  absl::optional<base::Value> json_value =
      base::JSONReader::Read(base::SysNSStringToUTF8(json_string));
  if (!json_value) {
    return nullptr;
  }
  return base::Value::ToUniquePtrValue(std::move(*json_value));
}

bool ExtractFormsData(NSString* forms_json,
                      bool filtered,
                      const std::u16string& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      const FieldDataManager& field_data_manager,
                      std::vector<FormData>* forms_data) {
  DCHECK(forms_data);
  std::unique_ptr<base::Value> forms_value = ParseJson(forms_json);
  if (!forms_value) {
    return false;
  }

  // Returned data should be a list of forms.
  if (!forms_value->is_list()) {
    return false;
  }

  // Iterate through all the extracted forms and copy the data from JSON into
  // BrowserAutofillManager structures.
  for (const auto& form_value : forms_value->GetList()) {
    const auto* form_dict = form_value.GetIfDict();
    if (!form_dict) {
      continue;
    }
    autofill::FormData form;
    if (ExtractFormData(*form_dict, filtered, form_name, main_frame_url,
                        frame_origin, field_data_manager, &form)) {
      forms_data->push_back(std::move(form));
    }
  }
  return true;
}

bool ExtractFormData(const base::Value::Dict& form,
                     bool filtered,
                     const std::u16string& form_name,
                     const GURL& main_frame_url,
                     const GURL& form_frame_origin,
                     const FieldDataManager& field_data_manager,
                     autofill::FormData* form_data) {
  DCHECK(form_data);
  // Form data is copied into a FormData object field-by-field.
  const std::string* name = form.FindString("name");
  if (!name) {
    return false;
  }
  form_data->name = base::UTF8ToUTF16(*name);
  if (filtered && form_name != form_data->name) {
    return false;
  }

  // Origin is mandatory.
  const std::string* origin_ptr = form.FindString("origin");
  if (!origin_ptr) {
    return false;
  }
  std::u16string origin = base::UTF8ToUTF16(*origin_ptr);

  // Use GURL object to verify origin of host frame URL.
  form_data->url = GURL(origin);
  if (form_data->url.DeprecatedGetOriginAsURL() != form_frame_origin) {
    return false;
  }

  // main_frame_origin is used for logging UKM.
  form_data->main_frame_origin = url::Origin::Create(main_frame_url);

  const std::string* unique_renderer_id = form.FindString("unique_renderer_id");
  if (unique_renderer_id && !unique_renderer_id->empty()) {
    StringToUint(*unique_renderer_id, &form_data->unique_renderer_id.value());
  } else {
    form_data->unique_renderer_id = FormRendererId();
  }

  // Action is optional.
  std::u16string action;
  if (const std::string* action_ptr = form.FindString("action")) {
    action = base::UTF8ToUTF16(*action_ptr);
  }
  form_data->action = GURL(action);

  // Optional fields.
  if (const std::string* name_attribute = form.FindString("name_attribute")) {
    form_data->name_attribute = base::UTF8ToUTF16(*name_attribute);
  }
  if (const std::string* id_attribute = form.FindString("id_attribute")) {
    form_data->id_attribute = base::UTF8ToUTF16(*id_attribute);
  }
  form_data->is_form_tag =
      form.FindBool("is_form_tag").value_or(form_data->is_form_tag);
  if (const std::string* frame_id = form.FindString("frame_id")) {
    form_data->frame_id = *frame_id;
  }

  // Field list (mandatory) is extracted.
  const base::Value::List* fields_list = form.FindList("fields");
  if (!fields_list) {
    return false;
  }
  for (const auto& field_dict : *fields_list) {
    autofill::FormFieldData field_data;
    if (field_dict.is_dict() &&
        ExtractFormFieldData(field_dict.GetDict(), field_data_manager,
                             &field_data)) {
      form_data->fields.push_back(std::move(field_data));
    } else {
      return false;
    }
  }
  return true;
}

bool ExtractFormFieldData(const base::Value::Dict& field,
                          const FieldDataManager& field_data_manager,
                          autofill::FormFieldData* field_data) {
  const std::string* name;
  const std::string* form_control_type;
  if (!(name = field.FindString("name")) ||
      !(form_control_type = field.FindString("form_control_type"))) {
    return false;
  }

  field_data->name = base::UTF8ToUTF16(*name);
  field_data->form_control_type = autofill::StringToFormControlTypeDiscouraged(
      *form_control_type, /*fallback=*/std::nullopt);

  const std::string* unique_renderer_id =
      field.FindString("unique_renderer_id");
  if (unique_renderer_id && !unique_renderer_id->empty()) {
    StringToUint(*unique_renderer_id, &field_data->unique_renderer_id.value());
  } else {
    field_data->unique_renderer_id = FieldRendererId();
  }

  // Optional fields.
  if (const std::string* name_attribute = field.FindString("name_attribute")) {
    field_data->name_attribute = base::UTF8ToUTF16(*name_attribute);
  }
  if (const std::string* id_attribute = field.FindString("id_attribute")) {
    field_data->id_attribute = base::UTF8ToUTF16(*id_attribute);
  }
  if (const std::string* label = field.FindString("label")) {
    field_data->label = base::UTF8ToUTF16(*label);
  }
  if (const std::string* value = field.FindString("value")) {
    field_data->value = base::UTF8ToUTF16(*value);
  }
  field_data->is_autofilled =
      field.FindBool("is_autofilled").value_or(field_data->is_autofilled);

  if (const std::string* autocomplete_attribute =
          field.FindString("autocomplete_attribute")) {
    field_data->autocomplete_attribute = *autocomplete_attribute;
  }
  if (absl::optional<int> max_length = field.FindInt("max_length")) {
    field_data->max_length = *max_length;
  }
  field_data->parsed_autocomplete =
      ParseAutocompleteAttribute(field_data->autocomplete_attribute);

  // TODO(crbug.com/427614): Extract |is_checked|.
  bool is_checkable = field.FindBool("is_checkable").value_or(false);
  autofill::SetCheckStatus(field_data, is_checkable, false);

  field_data->is_focusable =
      field.FindBool("is_focusable").value_or(field_data->is_focusable);
  field_data->should_autocomplete =
      field.FindBool("should_autocomplete")
          .value_or(field_data->should_autocomplete);

  if (const std::string* placeholder_attribute =
          field.FindString("placeholder_attribute")) {
    field_data->placeholder = base::UTF8ToUTF16(*placeholder_attribute);
  }

  if (const std::string* aria_label = field.FindString("aria_label")) {
    field_data->aria_label = base::UTF8ToUTF16(*aria_label);
  }
  if (const std::string* aria_description =
          field.FindString("aria_description")) {
    field_data->aria_description = base::UTF8ToUTF16(*aria_description);
  }

  // RoleAttribute::kOther is the default value. The only other value as of this
  // writing is RoleAttribute::kPresentation.
  absl::optional<int> role = field.FindInt("role");
  if (role &&
      *role == static_cast<int>(FormFieldData::RoleAttribute::kPresentation)) {
    field_data->role = FormFieldData::RoleAttribute::kPresentation;
  }

  // TODO(crbug.com/427614): Extract |text_direction|.

  // Load option values where present.
  const base::Value::List* option_values = field.FindList("option_values");
  const base::Value::List* option_contents = field.FindList("option_contents");
  if (option_values && option_contents) {
    if (option_values->size() != option_contents->size()) {
      return false;
    }
    auto value_it = option_values->begin();
    auto content_it = option_contents->begin();
    while (value_it != option_values->end() &&
           content_it != option_contents->end()) {
      if (value_it->is_string() && content_it->is_string()) {
        field_data->options.push_back(
            {.value = base::UTF8ToUTF16(value_it->GetString()),
             .content = base::UTF8ToUTF16(content_it->GetString())});
      }
      ++value_it;
      ++content_it;
    }
  }

  // Fill user input and properties mask.
  if (field_data_manager.HasFieldData(field_data->unique_renderer_id)) {
    field_data->user_input =
        field_data_manager.GetUserInput(field_data->unique_renderer_id);
    field_data->properties_mask = field_data_manager.GetFieldPropertiesMask(
        field_data->unique_renderer_id);
  }

  return true;
}

JavaScriptResultCallback CreateStringCallback(
    void (^completionHandler)(NSString*)) {
  return CreateStringCallback(base::BindOnce(completionHandler));
}

JavaScriptResultCallback CreateStringCallback(
    base::OnceCallback<void(NSString*)> callback) {
  return base::BindOnce(&ConvertValueToNSString, std::move(callback));
}

JavaScriptResultCallback CreateBoolCallback(void (^completionHandler)(BOOL)) {
  return CreateBoolCallback(base::BindOnce(completionHandler));
}

JavaScriptResultCallback CreateBoolCallback(
    base::OnceCallback<void(BOOL)> callback) {
  return base::BindOnce(&ConvertValueToBool, std::move(callback));
}

void ExecuteJavaScriptFunction(const std::string& name,
                               const base::Value::List& parameters,
                               web::WebFrame* frame,
                               JavaScriptResultCallback callback) {
  __block JavaScriptResultCallback cb = std::move(callback);

  if (!frame) {
    if (!cb.is_null()) {
      std::move(cb).Run(nil);
    }
    return;
  }
  if (!cb.is_null()) {
    bool called = frame->CallJavaScriptFunction(
        name, parameters, base::BindOnce(^(const base::Value* res) {
          std::move(cb).Run(res);
        }),
        base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
    if (!called) {
      std::move(cb).Run(nil);
    }
  } else {
    frame->CallJavaScriptFunction(name, parameters);
  }
}

bool ExtractIDs(NSString* json_string, std::vector<FieldRendererId>* ids) {
  DCHECK(ids);
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);
  if (!ids_value) {
    return false;
  }

  if (!ids_value->is_list()) {
    return false;
  }

  for (const auto& unique_id : ids_value->GetList()) {
    if (!unique_id.is_string()) {
      return false;
    }
    uint32_t id_num = 0;
    StringToUint(unique_id.GetString(), &id_num);
    ids->push_back(FieldRendererId(id_num));
  }
  return true;
}

bool ExtractFillingResults(
    NSString* json_string,
    std::map<uint32_t, std::u16string>* filling_results) {
  DCHECK(filling_results);
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);
  if (!ids_value || !ids_value->is_dict()) {
    return false;
  }

  for (const auto result : ids_value->GetDict()) {
    std::string id_string = result.first;
    uint32_t id_num = 0;
    StringToUint(id_string, &id_num);
    (*filling_results)[id_num] = base::UTF8ToUTF16(result.second.GetString());
  }
  return true;
}

}  // namespace autofill
