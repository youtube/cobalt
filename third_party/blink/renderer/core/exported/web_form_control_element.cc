/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_form_control_element.h"

#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "ui/events/keycodes/dom/dom_key.h"

#include "base/memory/scoped_refptr.h"

namespace blink {

bool WebFormControlElement::IsEnabled() const {
  return !ConstUnwrap<HTMLFormControlElement>()->IsDisabledFormControl();
}

bool WebFormControlElement::IsReadOnly() const {
  return ConstUnwrap<HTMLFormControlElement>()->IsReadOnly();
}

WebString WebFormControlElement::FormControlName() const {
  return ConstUnwrap<HTMLFormControlElement>()->GetName();
}

WebString WebFormControlElement::FormControlType() const {
  return ConstUnwrap<HTMLFormControlElement>()->type();
}

WebString WebFormControlElement::FormControlTypeForAutofill() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_)) {
    if (input->IsTextField() && input->HasBeenPasswordField())
      return input_type_names::kPassword;
  }

  return ConstUnwrap<HTMLFormControlElement>()->type();
}

WebAutofillState WebFormControlElement::GetAutofillState() const {
  return ConstUnwrap<HTMLFormControlElement>()->GetAutofillState();
}

bool WebFormControlElement::IsAutofilled() const {
  return ConstUnwrap<HTMLFormControlElement>()->IsAutofilled();
}

bool WebFormControlElement::UserHasEditedTheField() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->UserHasEditedTheField();
  if (auto* select_element = ::blink::DynamicTo<HTMLSelectElement>(*private_))
    return select_element->UserHasEditedTheField();
  if (auto* select_menu_element =
          ::blink::DynamicTo<HTMLSelectMenuElement>(*private_))
    return select_menu_element->UserHasEditedTheField();
  return true;
}

void WebFormControlElement::SetUserHasEditedTheField(bool value) {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    input->SetUserHasEditedTheField(value);
  if (auto* select_element = ::blink::DynamicTo<HTMLSelectElement>(*private_))
    select_element->SetUserHasEditedTheField(value);
  if (auto* select_menu_element =
          ::blink::DynamicTo<HTMLSelectMenuElement>(*private_))
    select_menu_element->SetUserHasEditedTheField(value);
}

void WebFormControlElement::SetUserHasEditedTheFieldForTest() {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    input->SetUserHasEditedTheFieldForTest();
}

void WebFormControlElement::SetAutofillState(WebAutofillState autofill_state) {
  Unwrap<HTMLFormControlElement>()->SetAutofillState(autofill_state);
}

void WebFormControlElement::SetPreventHighlightingOfAutofilledFields(
    bool prevent_highlighting) {
  Unwrap<HTMLFormControlElement>()->SetPreventHighlightingOfAutofilledFields(
      prevent_highlighting);
}

bool WebFormControlElement::PreventHighlightingOfAutofilledFields() const {
  return ConstUnwrap<HTMLFormControlElement>()
      ->PreventHighlightingOfAutofilledFields();
}

WebString WebFormControlElement::AutofillSection() const {
  return ConstUnwrap<HTMLFormControlElement>()->AutofillSection();
}

void WebFormControlElement::SetAutofillSection(const WebString& section) {
  Unwrap<HTMLFormControlElement>()->SetAutofillSection(section);
}

FormElementPiiType WebFormControlElement::GetFormElementPiiType() const {
  return ConstUnwrap<HTMLFormControlElement>()->GetFormElementPiiType();
}

void WebFormControlElement::SetFormElementPiiType(
    FormElementPiiType form_element_pii_type) {
  Unwrap<HTMLFormControlElement>()->SetFormElementPiiType(
      form_element_pii_type);
}

WebString WebFormControlElement::NameForAutofill() const {
  return ConstUnwrap<HTMLFormControlElement>()->NameForAutofill();
}

bool WebFormControlElement::AutoComplete() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->ShouldAutocomplete();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->ShouldAutocomplete();
  if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_))
    return select->ShouldAutocomplete();
  return false;
}

void WebFormControlElement::SetValue(const WebString& value, bool send_events) {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_)) {
    input->SetValue(value,
                    send_events
                        ? TextFieldEventBehavior::kDispatchInputAndChangeEvent
                        : TextFieldEventBehavior::kDispatchNoEvent);
  } else if (auto* textarea =
                 ::blink::DynamicTo<HTMLTextAreaElement>(*private_)) {
    textarea->SetValue(
        value, send_events
                   ? TextFieldEventBehavior::kDispatchInputAndChangeEvent
                   : TextFieldEventBehavior::kDispatchNoEvent);
  } else if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_)) {
    select->SetValue(value, send_events);
  }
}

void WebFormControlElement::DispatchFocusEvent() {
  Unwrap<Element>()->DispatchFocusEvent(
      nullptr, mojom::blink::FocusType::kForward, nullptr);
}

void WebFormControlElement::DispatchBlurEvent() {
  Unwrap<Element>()->DispatchBlurEvent(
      nullptr, mojom::blink::FocusType::kForward, nullptr);
}

void WebFormControlElement::SetAutofillValue(const WebString& value,
                                             WebAutofillState autofill_state) {
  // The input and change events will be sent in setValue.
  if (IsA<HTMLInputElement>(*private_) || IsA<HTMLTextAreaElement>(*private_)) {
    if (!Focused())
      DispatchFocusEvent();

    auto send_event = [local_dom_window =
                           Unwrap<Element>()->GetDocument().domWindow(),
                       this](WebInputEvent::Type event_type) {
      WebKeyboardEvent web_event{event_type, WebInputEvent::kNoModifiers,
                                 base::TimeTicks::Now()};
      web_event.dom_key = ui::DomKey::UNIDENTIFIED;
      web_event.dom_code = static_cast<int>(ui::DomKey::UNIDENTIFIED);
      web_event.native_key_code = blink::VKEY_UNKNOWN;
      web_event.windows_key_code = blink::VKEY_UNKNOWN;
      web_event.text[0] = blink::VKEY_UNKNOWN;
      web_event.unmodified_text[0] = blink::VKEY_UNKNOWN;

      KeyboardEvent* event = KeyboardEvent::Create(web_event, local_dom_window);
      Unwrap<Element>()->DispatchScopedEvent(*event);
    };

    // Simulate key events in case the website checks via JS that a keyboard
    // interaction took place.
    if (base::FeatureList::IsEnabled(
            blink::features::kAutofillSendUnidentifiedKeyAfterFill)) {
      send_event(WebInputEvent::Type::kRawKeyDown);
    } else {
      Unwrap<Element>()->DispatchScopedEvent(
          *Event::CreateBubble(event_type_names::kKeydown));
    }

    Unwrap<TextControlElement>()->SetAutofillValue(
        value, value.IsEmpty() ? WebAutofillState::kNotFilled : autofill_state);

    if (base::FeatureList::IsEnabled(
            blink::features::kAutofillSendUnidentifiedKeyAfterFill)) {
      send_event(WebInputEvent::Type::kChar);
      send_event(WebInputEvent::Type::kKeyUp);
    } else {
      Unwrap<Element>()->DispatchScopedEvent(
          *Event::CreateBubble(event_type_names::kKeyup));
    }

    if (!Focused())
      DispatchBlurEvent();
  } else if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_)) {
    if (!Focused())
      DispatchFocusEvent();
    select->SetAutofillValue(value, autofill_state);
    if (!Focused())
      DispatchBlurEvent();
  } else if (auto* selectmenu =
                 ::blink::DynamicTo<HTMLSelectMenuElement>(*private_)) {
    if (!Focused()) {
      DispatchFocusEvent();
    }
    selectmenu->SetAutofillValue(value, autofill_state);
    if (!Focused()) {
      DispatchBlurEvent();
    }
  }
}

WebString WebFormControlElement::Value() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->Value();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->Value();
  if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_))
    return select->Value();
  if (auto* selectmenu = ::blink::DynamicTo<HTMLSelectMenuElement>(*private_)) {
    return selectmenu->value();
  }
  return WebString();
}

void WebFormControlElement::SetSuggestedValue(const WebString& value) {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_)) {
    input->SetSuggestedValue(value);
  } else if (auto* textarea =
                 ::blink::DynamicTo<HTMLTextAreaElement>(*private_)) {
    textarea->SetSuggestedValue(value);
  } else if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_)) {
    select->SetSuggestedValue(value);
  }
}

WebString WebFormControlElement::SuggestedValue() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->SuggestedValue();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->SuggestedValue();
  if (auto* select = ::blink::DynamicTo<HTMLSelectElement>(*private_))
    return select->SuggestedValue();
  return WebString();
}

WebString WebFormControlElement::EditingValue() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->InnerEditorValue();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->InnerEditorValue();
  return WebString();
}

void WebFormControlElement::SetSelectionRange(int start, int end) {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    input->SetSelectionRange(start, end);
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    textarea->SetSelectionRange(start, end);
}

int WebFormControlElement::SelectionStart() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->selectionStart();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->selectionStart();
  return 0;
}

int WebFormControlElement::SelectionEnd() const {
  if (auto* input = ::blink::DynamicTo<HTMLInputElement>(*private_))
    return input->selectionEnd();
  if (auto* textarea = ::blink::DynamicTo<HTMLTextAreaElement>(*private_))
    return textarea->selectionEnd();
  return 0;
}

WebString WebFormControlElement::AlignmentForFormData() const {
  if (const ComputedStyle* style =
          ConstUnwrap<HTMLFormControlElement>()->GetComputedStyle()) {
    if (style->GetTextAlign() == ETextAlign::kRight)
      return WebString::FromUTF8("right");
    if (style->GetTextAlign() == ETextAlign::kLeft)
      return WebString::FromUTF8("left");
  }
  return WebString();
}

WebString WebFormControlElement::DirectionForFormData() const {
  if (const ComputedStyle* style =
          ConstUnwrap<HTMLFormControlElement>()->GetComputedStyle()) {
    return style->IsLeftToRightDirection() ? WebString::FromUTF8("ltr")
                                           : WebString::FromUTF8("rtl");
  }
  return WebString::FromUTF8("ltr");
}

WebFormElement WebFormControlElement::Form() const {
  return WebFormElement(ConstUnwrap<HTMLFormControlElement>()->Form());
}

uint64_t WebFormControlElement::UniqueRendererFormControlId() const {
  return ConstUnwrap<HTMLFormControlElement>()->UniqueRendererFormControlId();
}

int32_t WebFormControlElement::GetAxId() const {
  return ConstUnwrap<HTMLFormControlElement>()->GetAxId();
}

WebFormControlElement::WebFormControlElement(HTMLFormControlElement* elem)
    : WebElement(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebFormControlElement,
                           IsElementNode() &&
                               ConstUnwrap<Element>()->IsFormControlElement())

WebFormControlElement& WebFormControlElement::operator=(
    HTMLFormControlElement* elem) {
  private_ = elem;
  return *this;
}

WebFormControlElement::operator HTMLFormControlElement*() const {
  return blink::To<HTMLFormControlElement>(private_.Get());
}

}  // namespace blink
