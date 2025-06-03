/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/invoke_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_listbox_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tag_name,
                                               Document& document)
    : HTMLElement(tag_name, document),
      autofill_state_(WebAutofillState::kNotFilled),
      blocks_form_submission_(false),
      interacted_since_last_form_submit_(false) {
  SetHasCustomStyleCallbacks();
  static uint64_t next_free_unique_id = 1;
  unique_renderer_form_control_id_ = next_free_unique_id++;
}

HTMLFormControlElement::~HTMLFormControlElement() = default;

void HTMLFormControlElement::Trace(Visitor* visitor) const {
  ListedElement::Trace(visitor);
  HTMLElement::Trace(visitor);
}

String HTMLFormControlElement::formAction() const {
  const AtomicString& action = FastGetAttribute(html_names::kFormactionAttr);
  if (action.empty()) {
    return GetDocument().Url();
  }
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(action));
}

void HTMLFormControlElement::setFormAction(const AtomicString& value) {
  setAttribute(html_names::kFormactionAttr, value);
}

String HTMLFormControlElement::formEnctype() const {
  const AtomicString& form_enctype_attr =
      FastGetAttribute(html_names::kFormenctypeAttr);
  if (form_enctype_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::ParseEncodingType(form_enctype_attr);
}

void HTMLFormControlElement::setFormEnctype(const AtomicString& value) {
  setAttribute(html_names::kFormenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const {
  const AtomicString& form_method_attr =
      FastGetAttribute(html_names::kFormmethodAttr);
  if (form_method_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::MethodString(
      FormSubmission::Attributes::ParseMethodType(form_method_attr));
}

void HTMLFormControlElement::setFormMethod(const AtomicString& value) {
  setAttribute(html_names::kFormmethodAttr, value);
}

bool HTMLFormControlElement::FormNoValidate() const {
  return FastHasAttribute(html_names::kFormnovalidateAttr);
}

void HTMLFormControlElement::Reset() {
  SetAutofillState(WebAutofillState::kNotFilled);
  ResetImpl();
  SetInteractedSinceLastFormSubmit(false);
}

void HTMLFormControlElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == html_names::kDisabledAttr &&
      params.old_value.IsNull() != params.new_value.IsNull()) {
    DisabledAttributeChanged();
    if (params.reason == AttributeModificationReason::kDirectly &&
        IsDisabledFormControl() && AdjustedFocusedElementInTreeScope() == this)
      blur();
  }
}

void HTMLFormControlElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kFormAttr) {
    FormAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kFormAttribute);
  } else if (name == html_names::kReadonlyAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      ReadonlyAttributeChanged();
      PseudoStateChanged(CSSSelector::kPseudoReadOnly);
      PseudoStateChanged(CSSSelector::kPseudoReadWrite);
      InvalidateIfHasEffectiveAppearance();
    }
  } else if (name == html_names::kRequiredAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull())
      RequiredAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kRequiredAttribute);
  } else if (name == html_names::kAutofocusAttr) {
    HTMLElement::ParseAttribute(params);
    UseCounter::Count(GetDocument(), WebFeature::kAutoFocusAttribute);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLFormControlElement::DisabledAttributeChanged() {
  // Don't blur in this function because this is called for descendants of
  // <fieldset> while tree traversal.
  EventDispatchForbiddenScope event_forbidden;

  ListedElement::DisabledAttributeChanged();
  InvalidateIfHasEffectiveAppearance();

  // TODO(dmazzoni): http://crbug.com/699438.
  // Replace |CheckedStateChanged| with a generic tree changed event.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);
}

void HTMLFormControlElement::RequiredAttributeChanged() {
  SetNeedsValidityCheck();
  PseudoStateChanged(CSSSelector::kPseudoRequired);
  PseudoStateChanged(CSSSelector::kPseudoOptional);
  // TODO(dmazzoni): http://crbug.com/699438.
  // Replace |CheckedStateChanged| with a generic tree changed event.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);
}

bool HTMLFormControlElement::IsReadOnly() const {
  return FastHasAttribute(html_names::kReadonlyAttr);
}

bool HTMLFormControlElement::IsDisabledOrReadOnly() const {
  return IsDisabledFormControl() || IsReadOnly();
}

void HTMLFormControlElement::SetAutofillState(WebAutofillState autofill_state) {
  if (autofill_state == autofill_state_)
    return;

  autofill_state_ = autofill_state;
  PseudoStateChanged(CSSSelector::kPseudoAutofill);
  PseudoStateChanged(CSSSelector::kPseudoWebKitAutofill);
  PseudoStateChanged(CSSSelector::kPseudoAutofillSelected);
  PseudoStateChanged(CSSSelector::kPseudoAutofillPreviewed);
}

void HTMLFormControlElement::SetPreventHighlightingOfAutofilledFields(
    bool prevent_highlighting) {
  if (prevent_highlighting == prevent_highlighting_of_autofilled_fields_)
    return;

  prevent_highlighting_of_autofilled_fields_ = prevent_highlighting;
  PseudoStateChanged(CSSSelector::kPseudoAutofill);
  PseudoStateChanged(CSSSelector::kPseudoWebKitAutofill);
  PseudoStateChanged(CSSSelector::kPseudoAutofillSelected);
  PseudoStateChanged(CSSSelector::kPseudoAutofillPreviewed);
}

void HTMLFormControlElement::SetAutofillSection(const WebString& section) {
  autofill_section_ = section;
}

bool HTMLFormControlElement::IsAutocompleteEmailUrlOrPassword() const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, values,
                      ({AtomicString("username"), AtomicString("new-password"),
                        AtomicString("current-password"), AtomicString("url"),
                        AtomicString("email"), AtomicString("impp")}));
  const AtomicString& autocomplete =
      FastGetAttribute(html_names::kAutocompleteAttr);
  if (autocomplete.IsNull())
    return false;
  return values.Contains(autocomplete.LowerASCII());
}

const AtomicString& HTMLFormControlElement::autocapitalize() const {
  if (!FastGetAttribute(html_names::kAutocapitalizeAttr).empty())
    return HTMLElement::autocapitalize();

  // If the form control itself does not have the autocapitalize attribute set,
  // but the form owner is non-null and does have the autocapitalize attribute
  // set, we inherit from the form owner.
  if (HTMLFormElement* form = Form())
    return form->autocapitalize();

  return g_empty_atom;
}

void HTMLFormControlElement::DidMoveToNewDocument(Document& old_document) {
  ListedElement::DidMoveToNewDocument(old_document);
  HTMLElement::DidMoveToNewDocument(old_document);
}

Node::InsertionNotificationRequest HTMLFormControlElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  ListedElement::InsertedInto(insertion_point);
  return kInsertionDone;
}

void HTMLFormControlElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  ListedElement::RemovedFrom(insertion_point);
}

void HTMLFormControlElement::WillChangeForm() {
  ListedElement::WillChangeForm();
  if (formOwner() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
}

void HTMLFormControlElement::DidChangeForm() {
  ListedElement::DidChangeForm();
  if (formOwner() && isConnected() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
}

HTMLFormElement* HTMLFormControlElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLFormControlElement::IsDisabledFormControl() const {
  // Since the MHTML is loaded in sandboxing mode with form submission and
  // script execution disabled, we should gray out all form control elements
  // to indicate that the form cannot be worked on.
  if (GetDocument().Fetcher()->Archive())
    return true;

  return IsActuallyDisabled();
}

bool HTMLFormControlElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsRequired() const {
  return FastHasAttribute(html_names::kRequiredAttr);
}

String HTMLFormControlElement::ResultForDialogSubmit() {
  return FastGetAttribute(html_names::kValueAttr);
}

bool HTMLFormControlElement::SupportsFocus() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsKeyboardFocusable() const {
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return HTMLElement::IsKeyboardFocusable();

  // Form control elements are always keyboard focusable if they are focusable
  // at all, and don't have a negative tabindex set.
  return IsFocusable() && tabIndex() >= 0;
}

bool HTMLFormControlElement::MayTriggerVirtualKeyboard() const {
  return false;
}

bool HTMLFormControlElement::ShouldHaveFocusAppearance() const {
  return SelectorChecker::MatchesFocusVisiblePseudoClass(*this);
}

bool HTMLFormControlElement::willValidate() const {
  return ListedElement::WillValidate();
}

bool HTMLFormControlElement::MatchesValidityPseudoClasses() const {
  return willValidate();
}

bool HTMLFormControlElement::IsValidElement() {
  return ListedElement::IsValidElement();
}

bool HTMLFormControlElement::IsSuccessfulSubmitButton() const {
  return CanBeSuccessfulSubmitButton() && !IsDisabledFormControl();
}

// The element referenced by the `popovertarget` attribute is returned if a)
// that element exists, b) it is a valid Popover element, and c) this form
// control supports popover triggering. The return value will include the
// behavior, which is taken from the `popovertargetaction` attribute, and will
// be kNone unless there is a valid popover target.
HTMLFormControlElement::PopoverTargetElement
HTMLFormControlElement::popoverTargetElement() {
  const PopoverTargetElement no_element{.popover = nullptr,
                                        .action = PopoverTriggerAction::kNone};
  if (!IsInTreeScope() ||
      SupportsPopoverTriggering() == PopoverTriggerSupport::kNone ||
      IsDisabledFormControl() || (Form() && IsSuccessfulSubmitButton())) {
    return no_element;
  }

  Element* target_element;
  target_element = GetElementAttribute(html_names::kPopovertargetAttr);

  // The selectlist element's button which opens its listbox forms an implicit
  // popover trigger for the listbox in order to function properly with light
  // dismiss.
  if (!target_element && RuntimeEnabledFeatures::HTMLSelectListElementEnabled(
                             GetDocument().GetExecutionContext())) {
    if (auto* button = DynamicTo<HTMLButtonElement>(this)) {
      if (auto* selectlist = button->OwnerSelectList()) {
        target_element = selectlist->ListBoxPart();
      }
    }
  }

  if (!target_element) {
    return no_element;
  }
  auto* target_popover = DynamicTo<HTMLElement>(target_element);
  if (!target_popover || !target_popover->HasPopoverAttribute()) {
    return no_element;
  }
  // The default action is "toggle".
  PopoverTriggerAction action = PopoverTriggerAction::kToggle;
  auto action_value =
      getAttribute(html_names::kPopovertargetactionAttr).LowerASCII();
  if (action_value == "show") {
    action = PopoverTriggerAction::kShow;
  } else if (action_value == "hide") {
    action = PopoverTriggerAction::kHide;
  } else if (RuntimeEnabledFeatures::HTMLPopoverHintEnabled() &&
             action_value == "hover") {
    action = PopoverTriggerAction::kHover;
  }
  return PopoverTargetElement{.popover = target_popover, .action = action};
}

AtomicString HTMLFormControlElement::popoverTargetAction() const {
  auto attribute_value =
      FastGetAttribute(html_names::kPopovertargetactionAttr).LowerASCII();
  // ReflectEmpty="toggle", ReflectMissing="toggle"
  if (attribute_value.IsNull() || attribute_value.empty()) {
    return keywords::kToggle;
  } else if (attribute_value == keywords::kToggle ||
             attribute_value == keywords::kShow ||
             attribute_value == keywords::kHide) {
    return attribute_value;  // ReflectOnly
  } else if (RuntimeEnabledFeatures::HTMLPopoverHintEnabled() &&
             attribute_value == keywords::kHover) {
    return attribute_value;  // ReflectOnly (with HTMLPopoverHint enabled)
  } else {
    return keywords::kToggle;  // ReflectInvalid = "toggle"
  }
}
void HTMLFormControlElement::setPopoverTargetAction(const AtomicString& value) {
  setAttribute(html_names::kPopovertargetactionAttr, value);
}

AtomicString HTMLFormControlElement::invokeAction() const {
  DCHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());
  const AtomicString& attribute_value =
      FastGetAttribute(html_names::kInvokeactionAttr);
  if (attribute_value && !attribute_value.IsNull() &&
      !attribute_value.empty()) {
    return attribute_value;
  }
  return keywords::kAuto;
}
void HTMLFormControlElement::setInvokeAction(const AtomicString& value) {
  setAttribute(html_names::kInvokeactionAttr, value);
}

void HTMLFormControlElement::DefaultEventHandler(Event& event) {
  // Buttons that aren't form participants might be Invoker buttons or Popover
  // buttons.
  if (event.type() == event_type_names::kDOMActivate && IsInTreeScope() &&
      !IsDisabledFormControl() && (!Form() || !IsSuccessfulSubmitButton())) {
    HTMLElement* invokee = DynamicTo<HTMLElement>(
        GetElementAttribute(html_names::kInvoketargetAttr));
    auto popover = popoverTargetElement();

    // invoketarget & popovertarget shouldn't be combined, so warn.
    if (invokee && popover.popover) {
      ConsoleMessage* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "popovertarget is ignored on elements with invoketarget set.");
      console_message->SetNodes(GetDocument().GetFrame(),
                                {this->GetDomNodeId()});
      GetDocument().AddConsoleMessage(console_message);
    }

    // Buttons with an invoketarget will dispatch an InvokeEvent on the Invoker,
    // and run HandleInvokeInternal to perform default logic.
    if (invokee) {
      auto action = invokeAction();
      Event* invokeEvent =
          InvokeEvent::Create(event_type_names::kInvoke, action, this);
      invokee->DispatchEvent(*invokeEvent);
      if (!invokeEvent->defaultPrevented()) {
        invokee->HandleInvokeInternal(*this, action);
      }

    } else if (popover.popover) {
      // Buttons with a popovertarget will invoke popovers, which is the same
      // logic as an invoketarget with an appropriate invokeaction (e.g.
      // togglePopover), sans the `InvokeEvent` dispatch. Calling
      // `HandleInvokeInternal()` does not dispatch the event but can handle the
      // popover triggering logic. `popovertargetaction` must also be mapped
      // to the equivalent `invokeaction` string:
      //  popovertargetaction=auto -> invokeaction=auto
      //  popovertargetaction=toggle -> invokeaction=togglePopover
      //  popovertargetaction=show -> invokeaction=showPopover
      //  popovertargetaction=hide -> invokeaction=hidePopover
      // We must check to ensure the action is one of the avilable popover
      // invoker actions so that popovertargetaction cannot be set to something
      // like showModal.
      CHECK(!invokee);
      auto trigger_support = SupportsPopoverTriggering();
      CHECK_NE(trigger_support, PopoverTriggerSupport::kNone);
      CHECK_NE(popover.action, PopoverTriggerAction::kNone);
      AtomicString action;

      switch (popover.action) {
        case PopoverTriggerAction::kToggle:
          action = keywords::kTogglePopover;
          break;
        case PopoverTriggerAction::kShow:
          action = keywords::kShowPopover;
          break;
        case PopoverTriggerAction::kHide:
          action = keywords::kHidePopover;
          break;
        case PopoverTriggerAction::kHover:
          CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
          action = keywords::kShowPopover;
          break;
        case PopoverTriggerAction::kNone:
          NOTREACHED();
          break;
      }

      CHECK(action);
      popover.popover->HandleInvokeInternal(*this, action);
    }
  }
  HTMLElement::DefaultEventHandler(event);
}

void HTMLFormControlElement::SetHovered(bool hovered) {
  HandlePopoverInvokerHovered(hovered);
  HTMLElement::SetHovered(hovered);
}

void HTMLFormControlElement::HandlePopoverInvokerHovered(bool hovered) {
  if (!IsInTreeScope()) {
    return;
  }
  auto target_info = popoverTargetElement();
  auto target_popover = target_info.popover;
  if (!target_popover || target_info.action != PopoverTriggerAction::kHover) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());

  if (hovered) {
    // If we've just hovered an element (or the descendant of an element), see
    // if it has a popovertarget element set for hover triggering. If so, queue
    // a task to show the popover after a timeout.
    auto& hover_tasks = target_popover->GetPopoverData()->hoverShowTasks();
    CHECK(!hover_tasks.Contains(this));
    const ComputedStyle* computed_style = GetComputedStyle();
    if (!computed_style) {
      return;
    }
    float hover_delay_seconds = computed_style->PopoverShowDelay();
    // If the value is infinite or NaN, don't queue a task at all.
    CHECK_GE(hover_delay_seconds, 0);
    if (!std::isfinite(hover_delay_seconds)) {
      return;
    }
    // It's possible that multiple nested elements have popoverhovertarget
    // attributes pointing to the same popover, and in that case, we want to
    // trigger on the first of them that reaches its timeout threshold.
    hover_tasks.insert(
        this,
        PostDelayedCancellableTask(
            *GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault),
            FROM_HERE,
            WTF::BindOnce(
                [](HTMLFormControlElement* trigger_element,
                   HTMLElement* popover_element) {
                  if (!popover_element ||
                      !popover_element->HasPopoverAttribute()) {
                    return;
                  }
                  // Remove this element from hoverShowTasks always.
                  popover_element->GetPopoverData()->hoverShowTasks().erase(
                      trigger_element);
                  // Only trigger the popover if the popovertarget attribute
                  // still points to the same popover, and the popover is in the
                  // tree and still not showing.
                  auto current_target =
                      trigger_element->popoverTargetElement().popover;
                  if (popover_element->IsInTreeScope() &&
                      !popover_element->popoverOpen() &&
                      popover_element == current_target) {
                    popover_element->InvokePopover(*trigger_element);
                  }
                },
                WrapWeakPersistent(this),
                WrapWeakPersistent(target_popover.Get())),
            base::Seconds(hover_delay_seconds)));
  } else {
    // If we have a hover show task still waiting, cancel it. Based on this
    // logic, if you hover a popovertargetaction=hover element, then remove the
    // popovertarget attribute, there will be no way to stop the popover from
    // being shown after the delay, even if you subsequently de-hover the
    // element.
    if (auto& hover_tasks = target_popover->GetPopoverData()->hoverShowTasks();
        hover_tasks.Contains(this)) {
      hover_tasks.Take(this).Cancel();
    }
  }
}

// static
const HTMLFormControlElement*
HTMLFormControlElement::EnclosingFormControlElement(const Node* node) {
  if (!node)
    return nullptr;
  return Traversal<HTMLFormControlElement>::FirstAncestorOrSelf(*node);
}

String HTMLFormControlElement::NameForAutofill() const {
  String full_name = GetName();
  String trimmed_name = full_name.StripWhiteSpace();
  if (!trimmed_name.empty())
    return trimmed_name;
  full_name = GetIdAttribute();
  trimmed_name = full_name.StripWhiteSpace();
  return trimmed_name;
}

void HTMLFormControlElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    NodeCloningData& data) {
  HTMLElement::CloneNonAttributePropertiesFrom(source, data);
  SetNeedsValidityCheck();
}

void HTMLFormControlElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
}

int32_t HTMLFormControlElement::GetAxId() const {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View())
    return 0;
  // TODO(accessibility) Simplify this once AXIDs use DOMNodeIds. At that
  // point it will be safe to get the AXID at any time.
  if (AXObjectCache* cache = document.ExistingAXObjectCache()) {
    LocalFrameView* local_frame_view = document.View();
    if (local_frame_view->IsUpdatingLifecycle()) {
      // Autofill (the caller of this code) can end up making calls to get AXIDs
      // of form elements during, e.g. resize observer callbacks, which are
      // in the middle up updating the document lifecycle. In these cases, just
      // return the existing AXID of the element.
      return cache->GetExistingAXID(const_cast<HTMLFormControlElement*>(this));
    }

    return cache->GetAXID(const_cast<HTMLFormControlElement*>(this));
  }

  return 0;
}

void HTMLFormControlElement::SetInteractedSinceLastFormSubmit(
    bool interacted_since_last_form_submit) {
  if (interacted_since_last_form_submit_ == interacted_since_last_form_submit) {
    return;
  }
  interacted_since_last_form_submit_ = interacted_since_last_form_submit;
  PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  PseudoStateChanged(CSSSelector::kPseudoUserValid);
}

bool HTMLFormControlElement::MatchesUserInvalidPseudo() {
  return interacted_since_last_form_submit_ && MatchesValidityPseudoClasses() &&
         !ListedElement::IsValidElement();
}

bool HTMLFormControlElement::MatchesUserValidPseudo() {
  return interacted_since_last_form_submit_ && MatchesValidityPseudoClasses() &&
         ListedElement::IsValidElement();
}

}  // namespace blink
