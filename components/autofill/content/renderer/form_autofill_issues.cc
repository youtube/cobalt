// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_issues.h"

#include <vector>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebString;
using blink::WebVector;
using blink::mojom::GenericIssueErrorType;

namespace autofill::form_issues {

using form_util::IsAutofillableElement;

namespace {

constexpr base::StringPiece kFor = "for";
constexpr base::StringPiece kAriaLabelledBy = "aria-labelledby";
constexpr base::StringPiece kName = "name";
constexpr base::StringPiece kId = "id";
constexpr base::StringPiece kLabel = "label";
constexpr base::StringPiece kAutocomplete = "autocomplete";

// Wrapper for frequently used WebString constants.
template <const base::StringPiece& string>
const WebString& GetWebString() {
  static const base::NoDestructor<WebString> web_string(
      WebString::FromUTF8(string));
  return *web_string;
}

void MaybeAppendLabelWithoutControlDevtoolsIssue(
    WebLabelElement label,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  if (!label.CorrespondingControl().IsNull()) {
    return;
  }

  const WebString& for_attr = GetWebString<kFor>();
  if (!label.HasAttribute(for_attr)) {
    // Label has neither for attribute nor a control element was found.
    form_issues.emplace_back(
        GenericIssueErrorType::kFormLabelHasNeitherForNorNestedInput,
        label.GetDomNodeId());
  }
}

void MaybeAppendAriaLabelledByDevtoolsIssue(
    const WebElement& element,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  const WebString& aria_label_attr = GetWebString<kAriaLabelledBy>();
  if (base::ranges::any_of(
          base::SplitStringPiece(element.GetAttribute(aria_label_attr).Utf16(),
                                 base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY),
          [&](const auto& id) {
            return element.GetDocument().GetElementById(WebString(id)).IsNull();
          })) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormAriaLabelledByToNonExistingId,
        element.GetDomNodeId(), aria_label_attr);
  }
}

void MaybeAppendInputWithEmptyIdAndNameDevtoolsIssue(
    const WebFormControlElement& element,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  const WebString& name_attr = GetWebString<kName>();
  if (element.GetAttribute(name_attr).IsEmpty() &&
      element.GetIdAttribute().IsEmpty()) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormEmptyIdAndNameAttributesForInputError,
        element.GetDomNodeId());
  }
}

void MaybeAppendDuplicateIdForInputDevtoolsIssue(
    const WebVector<WebFormControlElement>& elements,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  const WebString& id_attr = GetWebString<kId>();

  // Create copies of |elements| with ids that can be modified
  WebVector<WebFormControlElement> elements_with_id_attr;
  elements_with_id_attr.reserve(elements.size());
  for (const auto& element : elements) {
    if (IsAutofillableElement(element) && !element.GetIdAttribute().IsEmpty()) {
      elements_with_id_attr.push_back(element);
    }
  }
  base::ranges::sort(elements_with_id_attr, {},
                     &WebFormControlElement::GetIdAttribute);

  for (auto it = elements_with_id_attr.begin();
       (it = base::ranges::adjacent_find(
            it, elements_with_id_attr.end(), {},
            &WebFormControlElement::GetIdAttribute)) !=
       elements_with_id_attr.end();
       it++) {
    bool current_element_not_added =
        form_issues.empty() ||
        form_issues.back().issue_type !=
            GenericIssueErrorType::kFormDuplicateIdForInputError ||
        form_issues.back().violating_node != it->GetDomNodeId();

    if (current_element_not_added) {
      form_issues.emplace_back(
          GenericIssueErrorType::kFormDuplicateIdForInputError,
          it->GetDomNodeId(), id_attr);
    }
    form_issues.emplace_back(
        GenericIssueErrorType::kFormDuplicateIdForInputError,
        std::next(it)->GetDomNodeId(), id_attr);
  }
}

void MaybeAppendAutocompleteAttributeDevtoolsIssue(
    const WebElement& element,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  const WebString& autocomplete_attr = GetWebString<kAutocomplete>();
  std::string autocomplete_attribute =
      form_util::GetAutocompleteAttribute(element);
  if (element.HasAttribute(autocomplete_attr) &&
      autocomplete_attribute.empty()) {
    form_issues.emplace_back(
        GenericIssueErrorType::kFormAutocompleteAttributeEmptyError,
        element.GetDomNodeId(), autocomplete_attr);
  }

  if (IsAutocompleteTypeWrongButWellIntended(autocomplete_attribute)) {
    form_issues.emplace_back(
        GenericIssueErrorType::
            kFormInputHasWrongButWellIntendedAutocompleteValueError,
        element.GetDomNodeId(), autocomplete_attr);
  }
}

void MaybeAppendInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
    const WebFormControlElement& element,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  const WebString& autocomplete_attr = GetWebString<kAutocomplete>();
  if (element.HasAttribute(autocomplete_attr)) {
    return;
  }

  auto ParsedHtmlAttributeValueToAutocompleteHasFieldType =
      [](const std::string& attribute_value) {
        absl::optional<AutocompleteParsingResult>
            parsed_attribute_to_autocomplete =
                ParseAutocompleteAttribute(attribute_value);
        if (!parsed_attribute_to_autocomplete) {
          return false;
        }

        return parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnspecified &&
               parsed_attribute_to_autocomplete->field_type !=
                   HtmlFieldType::kUnrecognized;
      };

  const WebString& name_attr = GetWebString<kName>();
  bool name_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetAttribute(name_attr).Utf8());
  bool id_attr_matches_autocomplete =
      ParsedHtmlAttributeValueToAutocompleteHasFieldType(
          element.GetIdAttribute().Utf8());

  if (name_attr_matches_autocomplete || id_attr_matches_autocomplete) {
    WebString attribute_with_autocomplete_value =
        id_attr_matches_autocomplete ? GetWebString<kId>() : name_attr;
    form_issues.emplace_back(
        GenericIssueErrorType::
            kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
        element.GetDomNodeId(), attribute_with_autocomplete_value);

    return;
  }
}

void AppendFormIssuesInternal(
    const WebVector<WebFormControlElement>& elements,
    std::vector<blink::WebAutofillClient::FormIssue>& form_issues) {
  if (elements.size() == 0) {
    return;
  }

  const WebString& label_attr = GetWebString<kLabel>();
  WebElementCollection labels =
      elements[0].GetDocument().GetElementsByHTMLTagName(label_attr);
  CHECK(!labels.IsNull());

  for (WebElement item = labels.FirstItem(); !item.IsNull();
       item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    MaybeAppendLabelWithoutControlDevtoolsIssue(label, form_issues);
  }

  MaybeAppendDuplicateIdForInputDevtoolsIssue(elements, form_issues);
  for (const WebFormControlElement& element : elements) {
    if (!form_util::IsAutofillableElement(element)) {
      continue;
    }

    MaybeAppendAriaLabelledByDevtoolsIssue(element, form_issues);
    MaybeAppendAutocompleteAttributeDevtoolsIssue(element, form_issues);
    MaybeAppendInputWithEmptyIdAndNameDevtoolsIssue(element, form_issues);
    MaybeAppendInputAssignedAutocompleteValueToIdOrNameAttributesDevtoolsIssue(
        element, form_issues);
  }
}

}  // namespace

std::vector<blink::WebAutofillClient::FormIssue> GetFormIssues(
    const blink::WebVector<blink::WebFormControlElement>& control_elements,
    std::vector<blink::WebAutofillClient::FormIssue> form_issues) {
  AppendFormIssuesInternal(control_elements, form_issues);
  return form_issues;
}

std::vector<blink::WebAutofillClient::FormIssue>
CheckForLabelsWithIncorrectForAttribute(
    const blink::WebDocument& document,
    const std::vector<FormFieldData>& fields,
    std::vector<blink::WebAutofillClient::FormIssue> form_issues) {
  const WebString& for_attr = GetWebString<kFor>();
  const WebString& label_attr = GetWebString<kLabel>();

  std::set<std::u16string> elements_whose_name_match_a_label_for_attr;
  for (const FormFieldData& field : fields) {
    if (field.label_source == FormFieldData::LabelSource::kForName) {
      elements_whose_name_match_a_label_for_attr.insert(field.name_attribute);
    }
  }

  WebElementCollection labels = document.GetElementsByHTMLTagName(label_attr);
  for (WebElement item = labels.FirstItem(); !item.IsNull();
       item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    if (!label.CorrespondingControl().IsNull() ||
        !label.HasAttribute(for_attr)) {
      continue;
    }

    if (elements_whose_name_match_a_label_for_attr.contains(
            label.GetAttribute(for_attr).Utf16())) {
      // Add a DevTools issue informing the developer that the `label`'s for-
      // attribute is pointing to the name of a field, even though the ID
      // should be used.
      form_issues.emplace_back(GenericIssueErrorType::kFormLabelForNameError,
                               label.GetDomNodeId(), for_attr);
    } else {
      // Label has for attribute but no labellable element whose id OR name
      // matches it.
      // This issue is not emitted in case an element has a name that matches
      // it, in this case we emit kFormLabelForNameError to educate developers
      // that labels should be linked to element ids.
      form_issues.emplace_back(
          GenericIssueErrorType::kFormLabelForMatchesNonExistingIdError,
          label.GetDomNodeId(), for_attr);
    }
  }
  return form_issues;
}

}  // namespace autofill::form_issues
