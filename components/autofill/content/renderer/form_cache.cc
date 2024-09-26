// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_form_analyser_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/common/metrics/form_element_pii_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebAutofillState;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

namespace {

blink::FormElementPiiType MapTypePredictionToFormElementPiiType(
    base::StringPiece type) {
  if (type == "NO_SERVER_DATA" || type == "UNKNOWN_TYPE" ||
      type == "EMPTY_TYPE" || type == "") {
    return blink::FormElementPiiType::kUnknown;
  }

  if (base::StartsWith(type, "EMAIL_"))
    return blink::FormElementPiiType::kEmail;
  if (base::StartsWith(type, "PHONE_"))
    return blink::FormElementPiiType::kPhone;
  return blink::FormElementPiiType::kOthers;
}

void LogDeprecationMessages(const WebFormControlElement& element) {
  std::string autocomplete_attribute =
      element.GetAttribute("autocomplete").Utf8();

  static const char* const deprecated[] = {"region", "locality"};
  for (const char* str : deprecated) {
    if (autocomplete_attribute.find(str) == std::string::npos)
      continue;
    std::string msg = base::StrCat(
        {"autocomplete='", str,
         "' is deprecated and will soon be ignored. See http://goo.gl/YjeSsW"});
    WebConsoleMessage console_message = WebConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kWarning, WebString::FromASCII(msg));
    element.GetDocument().GetFrame()->AddMessageToConsole(console_message);
  }
}

// Determines whether the form is interesting enough to be sent to the browser
// for further operations. This is the case if any of the below holds:
// (1) At least one field is editable.
// (2) At least one field has a non-empty autocomplete attribute.
// (3) There is at least one iframe.
bool IsFormInteresting(const FormData& form, size_t num_editable_elements) {
  DCHECK_GE(form.fields.size(), num_editable_elements);
  return num_editable_elements >= 1 || !form.child_frames.empty() ||
         base::ranges::any_of(form.fields, base::not_fn(&std::string::empty),
                              &FormFieldData::autocomplete_attribute);
}

void ClearSelectOrSelectMenuElement(
    WebFormControlElement& element,
    const std::map<FieldRendererId, std::u16string>& initial_values) {
  auto initial_value_iter = initial_values.find(
      FieldRendererId(element.UniqueRendererFormControlId()));
  if (initial_value_iter != initial_values.end() &&
      element.Value().Utf16() != initial_value_iter->second) {
    element.SetAutofillValue(
        blink::WebString::FromUTF16(initial_value_iter->second),
        blink::WebAutofillState::kNotFilled);
    element.SetUserHasEditedTheField(false);
  } else {
    element.SetAutofillState(WebAutofillState::kNotFilled);
  }
}

}  // namespace

FormCache::UpdateFormCacheResult::UpdateFormCacheResult() = default;
FormCache::UpdateFormCacheResult::UpdateFormCacheResult(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult& FormCache::UpdateFormCacheResult::operator=(
    UpdateFormCacheResult&&) = default;
FormCache::UpdateFormCacheResult::~UpdateFormCacheResult() = default;

FormCache::FormCache(WebLocalFrame* frame) : frame_(frame) {}
FormCache::~FormCache() = default;

FormCache::UpdateFormCacheResult FormCache::UpdateFormCache(
    const FieldDataManager* field_data_manager) {
  initial_checked_state_.clear();
  initial_select_values_.clear();
  initial_selectmenu_values_.clear();

  std::set<FieldRendererId> observed_unique_renderer_ids;

  // Log an error message for deprecated attributes, but only the first time
  // the form is parsed.
  bool log_deprecation_messages = parsed_forms_.empty();

  // |parsed_forms_| is re-populated below in ProcessForm().
  std::map<FormRendererId, FormData> old_parsed_forms =
      std::move(parsed_forms_);
  parsed_forms_.clear();

  UpdateFormCacheResult r;
  r.removed_forms = base::MakeFlatSet<FormRendererId>(
      old_parsed_forms, {}, &std::pair<const FormRendererId, FormData>::first);

  size_t num_fields_seen = 0;
  size_t num_frames_seen = 0;

  // Helper function that stores new autofillable forms in |forms|. Returns
  // false iff the total number of fields exceeds |kMaxParseableFields|. Clears
  // |form|'s FormData::child_frames if the total number of frames exceeds
  // kMaxParseableChildFrames.
  auto ProcessForm =
      [&](FormData form,
          const std::vector<WebFormControlElement>& control_elements) {
        for (const auto& field : form.fields)
          observed_unique_renderer_ids.insert(field.unique_renderer_id);

        num_fields_seen += form.fields.size();
        num_frames_seen += form.child_frames.size();

        // Enforce the kMaxParseableFields limit: ignore all forms after this
        // limit has been reached (i.e., abort parsing).
        if (num_fields_seen > kMaxParseableFields)
          return false;

        // Enforce the kMaxParseableChildFrames limit: ignore the iframes, but
        // do not ignore the fields (i.e., continue parsing).
        if (num_frames_seen > kMaxParseableChildFrames)
          form.child_frames.clear();

        size_t num_editable_elements =
            ScanFormControlElements(control_elements, log_deprecation_messages);

        // Store only forms that contain iframes or fields.
        if (IsFormInteresting(form, num_editable_elements)) {
          FormRendererId form_id = form.unique_renderer_id;
          DCHECK(parsed_forms_.find(form_id) == parsed_forms_.end());
          auto it = old_parsed_forms.find(form_id);
          if (it == old_parsed_forms.end() ||
              !FormData::DeepEqual(std::move(it->second), form)) {
            SaveInitialValues(control_elements);
            r.updated_forms.push_back(form);
          }
          r.removed_forms.erase(form_id);
          parsed_forms_[form_id] = std::move(form);
        }
        return true;
      };

  constexpr form_util::ExtractMask extract_mask =
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTIONS);

  WebDocument document = frame_->GetDocument();
  if (document.IsNull())
    return r;

  for (const WebFormElement& form_element : document.Forms()) {
    FormData form;
    if (!WebFormElementToFormData(form_element, WebFormControlElement(),
                                  field_data_manager, extract_mask, &form,
                                  nullptr)) {
      continue;
    }
    if (!ProcessForm(
            std::move(form),
            form_util::ExtractAutofillableElementsInForm(form_element))) {
      PruneInitialValueCaches(observed_unique_renderer_ids);
      return r;
    }
  }

  // Look for more parseable fields outside of forms. Create a synthetic form
  // from them.
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document);
  std::vector<WebElement> iframe_elements =
      form_util::GetUnownedIframeElements(document);

  FormData synthetic_form;
  if (!UnownedFormElementsToFormData(control_elements, iframe_elements, nullptr,
                                     document, field_data_manager, extract_mask,
                                     &synthetic_form, nullptr)) {
    PruneInitialValueCaches(observed_unique_renderer_ids);
    return r;
  }
  if (!ProcessForm(std::move(synthetic_form), control_elements)) {
    PruneInitialValueCaches(observed_unique_renderer_ids);
    return r;
  }

  PruneInitialValueCaches(observed_unique_renderer_ids);
  return r;
}

void FormCache::Reset() {
  synthetic_form_ = FormData();
  parsed_forms_.clear();
  initial_select_values_.clear();
  initial_selectmenu_values_.clear();
  initial_checked_state_.clear();
  fields_eligible_for_manual_filling_.clear();
}

void FormCache::ClearElement(WebFormControlElement& control_element,
                             const WebFormControlElement& trigger_element) {
  // Don't modify the value of disabled fields.
  if (!control_element.IsEnabled())
    return;

  // Don't clear the fields that were not autofilled.
  if (!control_element.IsAutofilled())
    return;

  if (control_element.AutofillSection() != trigger_element.AutofillSection())
    return;

  if (!form_util::IsAutofillableElement(control_element)) {
    // TODO(crbug.com/1336051): Handle selectmenu case and make this NOTREACHED.
    CHECK(form_util::IsSelectMenuElement(control_element));
    return;
  }

  WebInputElement web_input_element =
      control_element.DynamicTo<WebInputElement>();
  if (form_util::IsTextInput(web_input_element) ||
      form_util::IsMonthInput(web_input_element)) {
    web_input_element.SetAutofillValue(blink::WebString(),
                                       WebAutofillState::kNotFilled);

    // Clearing the value in the focused node (above) can cause the selection
    // to be lost. We force the selection range to restore the text cursor.
    if (trigger_element == web_input_element) {
      size_t length = web_input_element.Value().length();
      web_input_element.SetSelectionRange(length, length);
    }
  } else if (form_util::IsTextAreaElement(control_element)) {
    control_element.SetAutofillValue(blink::WebString(),
                                     WebAutofillState::kNotFilled);
  } else if (form_util::IsSelectElement(control_element)) {
    ClearSelectOrSelectMenuElement(control_element, initial_select_values_);
  } else if (form_util::IsSelectMenuElement(control_element)) {
    ClearSelectOrSelectMenuElement(control_element, initial_selectmenu_values_);
  } else if (form_util::IsCheckableElement(web_input_element)) {
    WebInputElement input_element = control_element.To<WebInputElement>();
    auto checkable_element_it = initial_checked_state_.find(
        FieldRendererId(input_element.UniqueRendererFormControlId()));
    if (checkable_element_it != initial_checked_state_.end() &&
        input_element.IsChecked() != checkable_element_it->second) {
      input_element.SetChecked(checkable_element_it->second, true,
                               WebAutofillState::kNotFilled);
    }
  } else {
    NOTREACHED();
  }
}

bool FormCache::ClearSectionWithElement(const WebFormControlElement& element) {
  // The intended behaviour is:
  // * Clear the currently focused element.
  // * Send the blur event.
  // * For each other element, focus -> clear -> blur.
  // * Send the focus event.
  WebFormElement form_element = element.Form();
  std::vector<WebFormControlElement> control_elements =
      form_element.IsNull()
          ? form_util::GetUnownedAutofillableFormFieldElements(
                element.GetDocument())
          : form_util::ExtractAutofillableElementsInForm(form_element);

  if (control_elements.empty())
    return true;

  if (control_elements.size() < 2 && control_elements[0].Focused()) {
    // If there is no other field to be cleared, sending the blur event and then
    // the focus event for the currently focused element does not make sense.
    ClearElement(control_elements[0], element);
    return true;
  }

  WebFormControlElement* initially_focused_element = nullptr;
  for (WebFormControlElement& control_element : control_elements) {
    if (control_element.Focused()) {
      initially_focused_element = &control_element;
      ClearElement(control_element, element);
      // A blur event is emitted for the focused element if it is an initiating
      // element before the clearing happens.
      initially_focused_element->DispatchBlurEvent();
      break;
    }
  }

  for (WebFormControlElement& control_element : control_elements) {
    if (control_element.Focused())
      continue;
    ClearElement(control_element, element);
  }

  // A focus event is emitted for the initiating element after clearing is
  // completed.
  if (initially_focused_element)
    initially_focused_element->DispatchFocusEvent();

  return true;
}

bool FormCache::ShowPredictions(const FormDataPredictions& form,
                                bool attach_predictions_to_dom) {
  DCHECK_EQ(form.data.fields.size(), form.fields.size());

  std::vector<WebFormControlElement> control_elements;

  if (form.data.unique_renderer_id.is_null()) {  // Form is synthetic.
    WebDocument document = frame_->GetDocument();
    control_elements =
        form_util::GetUnownedAutofillableFormFieldElements(document);
  } else {
    for (const WebFormElement& form_element : frame_->GetDocument().Forms()) {
      FormRendererId form_id(form_element.UniqueRendererFormId());
      if (form_id == form.data.unique_renderer_id) {
        control_elements =
            form_util::ExtractAutofillableElementsInForm(form_element);
        break;
      }
    }
  }

  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  PageFormAnalyserLogger logger(frame_);
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement& element = control_elements[i];

    const FormFieldData& field_data = form.data.fields[i];
    FieldRendererId field_id(element.UniqueRendererFormControlId());
    if (field_id != field_data.unique_renderer_id)
      continue;
    const FormFieldDataPredictions& field = form.fields[i];

    element.SetFormElementPiiType(
        MapTypePredictionToFormElementPiiType(field.overall_type));

    // If the flag is enabled, attach the prediction to the field.
    if (attach_predictions_to_dom) {
      constexpr size_t kMaxLabelSize = 100;
      // TODO(crbug/1165780): Use `parseable_label()` once the feature is
      // launched.
      const std::u16string truncated_label =
          field_data.label.substr(0, kMaxLabelSize);

      std::string form_id =
          base::NumberToString(form.data.unique_renderer_id.value());
      std::string field_id_str =
          base::NumberToString(field_data.unique_renderer_id.value());

      blink::LocalFrameToken frame_token;
      if (auto* frame = element.GetDocument().GetFrame())
        frame_token = frame->GetLocalFrameToken();

      std::string title = base::StrCat({
          "overall type: ",
          field.overall_type,
          "\nserver type: ",
          field.server_type,
          "\nheuristic type: ",
          field.heuristic_type,
          "\nlabel: ",
          base::UTF16ToUTF8(truncated_label),
          "\nparseable name: ",
          field.parseable_name,
          "\nsection: ",
          field.section,
          "\nfield signature: ",
          field.signature,
          "\nform signature: ",
          form.signature,
          "\nform signature in host form: ",
          field.host_form_signature,
          "\nfield frame token: ",
          frame_token.ToString(),
          "\nform renderer id: ",
          form_id,
          "\nfield renderer id: ",
          field_id_str,
          "\nvisible: ",
          field_data.is_visible ? "true" : "false",
          "\nfocusable: ",
          field_data.IsFocusable() ? "true" : "false",
          "\nfield rank: ",
          base::NumberToString(field.rank),
          "\nfield rank in signature group: ",
          base::NumberToString(field.rank_in_signature_group),
          "\nfield rank in host form: ",
          base::NumberToString(field.rank_in_host_form),
          "\nfield rank in host form signature group: ",
          base::NumberToString(field.rank_in_host_form_signature_group),
      });

      WebString kAutocomplete = WebString::FromASCII("autocomplete");
      if (element.HasAttribute(kAutocomplete)) {
        title += "\nautocomplete: " +
                 element.GetAttribute(kAutocomplete).Utf8().substr(0, 100);
      }

      // Set this debug string to the title so that a developer can easily debug
      // by hovering the mouse over the input field.
      element.SetAttribute("title", WebString::FromUTF8(title));

      // Set the same debug string to an attribute that does not get mangled if
      // Google Translate is triggered for the site. This is useful for
      // automated processing of the data.
      element.SetAttribute("autofill-information", WebString::FromUTF8(title));

      element.SetAttribute("autofill-prediction",
                           WebString::FromUTF8(field.overall_type));
    }
  }
  logger.Flush();

  return true;
}

void FormCache::SetFieldsEligibleForManualFilling(
    const std::vector<FieldRendererId>& fields_eligible_for_manual_filling) {
  fields_eligible_for_manual_filling_ = base::flat_set<FieldRendererId>(
      std::move(fields_eligible_for_manual_filling));
}

size_t FormCache::ScanFormControlElements(
    const std::vector<WebFormControlElement>& control_elements,
    bool log_deprecation_messages) {
  size_t num_editable_elements = 0;
  for (const WebFormControlElement& element : control_elements) {
    if (log_deprecation_messages)
      LogDeprecationMessages(element);

    // Save original values of <select> elements so we can restore them
    // when |ClearFormWithNode()| is invoked.
    if (form_util::IsSelectElement(element) ||
        form_util::IsTextAreaElement(element)) {
      ++num_editable_elements;
    } else if (!form_util::IsSelectMenuElement(element) &&
               !form_util::IsCheckableElement(element)) {
      ++num_editable_elements;
    }
    // TODO(crbug.com/1336051): Handle selectmenu case.
  }
  return num_editable_elements;
}

void FormCache::SaveInitialValues(
    const std::vector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& element : control_elements) {
    if (form_util::IsSelectElement(element)) {
      initial_select_values_.insert(
          {FieldRendererId(element.UniqueRendererFormControlId()),
           element.Value().Utf16()});
    } else if (form_util::IsSelectMenuElement(element)) {
      initial_selectmenu_values_.insert(
          {FieldRendererId(element.UniqueRendererFormControlId()),
           element.Value().Utf16()});
    } else if (form_util::IsCheckableElement(element)) {
      const WebInputElement input_element = element.To<WebInputElement>();
      initial_checked_state_.insert(
          {FieldRendererId(input_element.UniqueRendererFormControlId()),
           input_element.IsChecked()});
    }
  }
}

void FormCache::PruneInitialValueCaches(
    const std::set<FieldRendererId>& ids_to_retain) {
  auto should_not_retain = [&ids_to_retain](const auto& p) {
    return !base::Contains(ids_to_retain, p.first);
  };
  base::EraseIf(initial_select_values_, should_not_retain);
  base::EraseIf(initial_selectmenu_values_, should_not_retain);
  base::EraseIf(initial_checked_state_, should_not_retain);
}

}  // namespace autofill
