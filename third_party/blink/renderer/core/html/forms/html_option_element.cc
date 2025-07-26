/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class OptionTextObserver : public MutationObserver::Delegate {
 public:
  explicit OptionTextObserver(HTMLOptionElement& option)
      : option_(option), observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setCharacterData(true);
    init->setChildList(true);
    init->setSubtree(true);
    observer_->observe(option_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return option_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    option_->DidChangeTextContent();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(option_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<HTMLOptionElement> option_;
  Member<MutationObserver> observer_;
};

HTMLOptionElement::HTMLOptionElement(Document& document)
    : HTMLElement(html_names::kOptionTag, document), is_selected_(false) {
  EnsureUserAgentShadowRoot();
}

// An explicit empty destructor should be in html_option_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_option_element.h, when including html_option_element.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptionElement::~HTMLOptionElement() = default;

HTMLOptionElement* HTMLOptionElement::CreateForJSConstructor(
    Document& document,
    const String& data,
    const AtomicString& value,
    bool default_selected,
    bool selected,
    ExceptionState& exception_state) {
  HTMLOptionElement* element =
      MakeGarbageCollected<HTMLOptionElement>(document);
  element->EnsureUserAgentShadowRoot();
  if (!data.empty()) {
    element->AppendChild(Text::Create(document, data), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (!value.IsNull())
    element->setValue(value);
  if (default_selected)
    element->setAttribute(html_names::kSelectedAttr, g_empty_atom);
  element->SetSelected(selected);

  return element;
}

void HTMLOptionElement::Trace(Visitor* visitor) const {
  visitor->Trace(text_observer_);
  visitor->Trace(nearest_ancestor_select_);
  visitor->Trace(label_container_);
  HTMLElement::Trace(visitor);
}

FocusableState HTMLOptionElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList()) {
    auto* popover = select->PopoverForAppearanceBase();
    if (popover && popover->popoverOpen()) {
      // If this option is being rendered as regular web content inside a
      // base-select <select> popover, then we need this element to be
      // focusable.
      return IsDisabledFormControl() ? FocusableState::kNotFocusable
                                     : FocusableState::kFocusable;
    }
    return FocusableState::kNotFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

bool HTMLOptionElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kSelectedAttr);
}

bool HTMLOptionElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

String HTMLOptionElement::DisplayLabel() const {
  if (RuntimeEnabledFeatures::OptionLabelAttributeWhitespaceEnabled()) {
    // If the label attribute is set and is not an empty string, then use its
    // value. Otherwise, use inner text.
    String label_attr = String(FastGetAttribute(html_names::kLabelAttr));
    if (!label_attr.empty()) {
      return label_attr.StripWhiteSpace(IsHTMLSpace<UChar>)
          .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
    }
    return CollectOptionInnerText()
        .StripWhiteSpace(IsHTMLSpace<UChar>)
        .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  }

  String label_attr = String(FastGetAttribute(html_names::kLabelAttr))
    .StripWhiteSpace(IsHTMLSpace<UChar>).SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  String inner_text = CollectOptionInnerText()
    .StripWhiteSpace(IsHTMLSpace<UChar>).SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  // FIXME: The following treats an element with the label attribute set to
  // the empty string the same as an element with no label attribute at all.
  // Is that correct? If it is, then should the label function work the same
  // way?
  return label_attr.empty() ? inner_text : label_attr;
}

String HTMLOptionElement::text() const {
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setText(const String& text) {
  // Changing the text causes a recalc of a select's items, which will reset the
  // selected index to the first item if the select is single selection with a
  // menu list.  We attempt to preserve the selected item.
  HTMLSelectElement* select = OwnerSelectElement();
  bool select_is_menu_list = select && select->UsesMenuList();
  int old_selected_index = select_is_menu_list ? select->selectedIndex() : -1;

  setTextContent(text);

  if (select_is_menu_list && select->selectedIndex() != old_selected_index)
    select->setSelectedIndex(old_selected_index);
}

void HTMLOptionElement::AccessKeyAction(SimulatedClickCreationScope) {
  // TODO(crbug.com/1176745): why creation_scope arg is not used at all?
  if (HTMLSelectElement* select = OwnerSelectElement())
    select->SelectOptionByAccessKey(this);
}

int HTMLOptionElement::index() const {
  // It would be faster to cache the index, but harder to get it right in all
  // cases.

  HTMLSelectElement* select_element = OwnerSelectElement();
  if (!select_element)
    return 0;

  int option_index = 0;
  for (const auto& option : select_element->GetOptionList()) {
    if (option == this)
      return option_index;
    ++option_index;
  }

  return 0;
}

int HTMLOptionElement::ListIndex() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->ListIndexForOption(*this);
  return -1;
}

void HTMLOptionElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kValueAttr) {
    if (HTMLDataListElement* data_list = OwnerDataListElement()) {
      data_list->OptionElementChildrenChanged();
    }
    if (HTMLSelectElement* select = OwnerSelectElement()) {
      select->SetNeedsValidityCheck();
    }
  } else if (name == html_names::kDisabledAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      PseudoStateChanged(CSSSelector::kPseudoDisabled);
      PseudoStateChanged(CSSSelector::kPseudoEnabled);
      InvalidateIfHasEffectiveAppearance();
    }
  } else if (name == html_names::kSelectedAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull() && !is_dirty_)
      SetSelected(!params.new_value.IsNull());
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else if (name == html_names::kLabelAttr) {
    if (HTMLSelectElement* select = OwnerSelectElement())
      select->OptionElementChildrenChanged(*this);
    UpdateLabel();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

String HTMLOptionElement::value() const {
  const AtomicString& value = FastGetAttribute(html_names::kValueAttr);
  if (!value.IsNull())
    return value;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setValue(const AtomicString& value) {
  setAttribute(html_names::kValueAttr, value);
}

bool HTMLOptionElement::Selected() const {
  return is_selected_;
}

void HTMLOptionElement::SetSelected(bool selected) {
  if (is_selected_ == selected)
    return;

  SetSelectedState(selected);

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->OptionSelectionStateChanged(this, selected);
  }
}

bool HTMLOptionElement::selectedForBinding() const {
  return Selected();
}

void HTMLOptionElement::setSelectedForBinding(bool selected) {
  bool was_selected = is_selected_;
  SetSelected(selected);

  // As of December 2015, the HTML specification says the dirtiness becomes
  // true by |selected| setter unconditionally. However it caused a real bug,
  // crbug.com/570367, and is not compatible with other browsers.
  // Firefox seems not to set dirtiness if an option is owned by a select
  // element and selectedness is not changed.
  if (OwnerSelectElement() && was_selected == is_selected_)
    return;

  is_dirty_ = true;
}

void HTMLOptionElement::SetSelectedState(bool selected) {
  if (is_selected_ == selected)
    return;

  is_selected_ = selected;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->InvalidateSelectedItems();

    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      // If there is a layoutObject (most common), fire accessibility
      // notifications only when it's a listbox (and not a menu list). If
      // there's no layoutObject, fire them anyway just to be safe (to make sure
      // the AX tree is in sync).
      if (!select->GetLayoutObject() || !select->UsesMenuList()) {
        cache->ListboxOptionStateChanged(this);
        cache->ListboxSelectedChildrenChanged(select);
      }
    }
  }
}

void HTMLOptionElement::SetMultiSelectFocusedState(bool focused) {
  if (is_multi_select_focused_ == focused)
    return;

  if (auto* select = OwnerSelectElement()) {
    DCHECK(select->IsMultiple());
    is_multi_select_focused_ = focused;
    PseudoStateChanged(CSSSelector::kPseudoMultiSelectFocus);
  }
}

bool HTMLOptionElement::IsMultiSelectFocused() const {
  return is_multi_select_focused_;
}

void HTMLOptionElement::SetDirty(bool value) {
  is_dirty_ = value;
}

void HTMLOptionElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  DidChangeTextContent();

  // If an element is inserted, We need to use MutationObserver to detect
  // textContent changes.
  if (change.type == ChildrenChangeType::kElementInserted && !text_observer_)
    text_observer_ = MakeGarbageCollected<OptionTextObserver>(*this);
}

void HTMLOptionElement::DidChangeTextContent() {
  if (HTMLDataListElement* data_list = OwnerDataListElement()) {
    data_list->OptionElementChildrenChanged();
  }
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->OptionElementChildrenChanged(*this);
  }
  UpdateLabel();
}

HTMLDataListElement* HTMLOptionElement::OwnerDataListElement() const {
  return Traversal<HTMLDataListElement>::FirstAncestor(*this);
}

namespace {
HTMLSelectElement* NearestAncestorSelectNoNesting(
    const HTMLOptionElement& option) {
  for (Node& ancestor : NodeTraversal::AncestorsOf(option)) {
    if (IsA<HTMLOptionElement>(ancestor)) {
      // Don't associate nested <option>s with <select>s. This matches the
      // traversals in OptionList and HTMLOptionElement::InsertedInto.
      return nullptr;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      return select;
    }
  }
  return nullptr;
}
}  // namespace

HTMLSelectElement* HTMLOptionElement::OwnerSelectElement(
    bool skip_check) const {
  if (HTMLSelectElement::SelectParserRelaxationEnabled(this)) {
    if (!skip_check) {
      DCHECK_EQ(nearest_ancestor_select_,
                NearestAncestorSelectNoNesting(*this));
    }
    return nearest_ancestor_select_;
  } else {
    if (!parentNode()) {
      return nullptr;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(*parentNode())) {
      return select;
    }
    if (IsA<HTMLOptGroupElement>(*parentNode())) {
      return DynamicTo<HTMLSelectElement>(parentNode()->parentNode());
    }
  }
  return nullptr;
}

void HTMLOptionElement::SetOwnerSelectElement(HTMLSelectElement* select) {
  CHECK(HTMLSelectElement::SelectParserRelaxationEnabled(this));
  DCHECK_EQ(select, NearestAncestorSelectNoNesting(*this));
  nearest_ancestor_select_ = select;
}

String HTMLOptionElement::label() const {
  const AtomicString& label = FastGetAttribute(html_names::kLabelAttr);
  if (!label.IsNull())
    return label;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setLabel(const AtomicString& label) {
  setAttribute(html_names::kLabelAttr, label);
}

String HTMLOptionElement::TextIndentedToRespectGroupLabel() const {
  ContainerNode* parent = parentNode();
  if (parent && IsA<HTMLOptGroupElement>(*parent))
    return "    " + DisplayLabel();
  return DisplayLabel();
}

bool HTMLOptionElement::OwnElementDisabled() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

bool HTMLOptionElement::IsDisabledFormControl() const {
  if (OwnElementDisabled())
    return true;
  if (Element* parent = parentElement())
    return IsA<HTMLOptGroupElement>(*parent) && parent->IsDisabledFormControl();
  return false;
}

String HTMLOptionElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

String HTMLOptionElement::CollectOptionInnerText() const {
  StringBuilder text;
  for (Node* node = firstChild(); node;) {
    if (node->IsTextNode())
      text.Append(node->nodeValue());
    // Text nodes inside script elements are not part of the option text.
    auto* element = DynamicTo<Element>(node);
    if (element && element->IsScriptElement())
      node = NodeTraversal::NextSkippingChildren(*node, this);
    else
      node = NodeTraversal::Next(*node, this);
  }
  return text.ToString();
}

HTMLFormElement* HTMLOptionElement::form() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->formOwner();

  return nullptr;
}

void HTMLOptionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  if (HTMLSelectElement::CustomizableSelectEnabled(this)) {
    label_container_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
    label_container_->SetShadowPseudoId(
        shadow_element_names::kOptionLabelContainer);
    label_container_->setAttribute(html_names::kAriaHiddenAttr,
                                   keywords::kTrue);
    root.appendChild(label_container_);

    auto* slot = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
    slot->SetShadowPseudoId(shadow_element_names::kOptionSlot);
    root.appendChild(slot);
  }

  UpdateLabel();
}

void HTMLOptionElement::UpdateLabel() {
  if (HTMLSelectElement::CustomizableSelectEnabled(this)) {
    if (label_container_) {
      label_container_->setTextContent(DisplayLabel());
    }
  } else if (ShadowRoot* root = UserAgentShadowRoot()) {
    root->setTextContent(DisplayLabel());
  }
}

Node::InsertionNotificationRequest HTMLOptionElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);
  if (!HTMLSelectElement::SelectParserRelaxationEnabled(this)) {
    CHECK(!HTMLSelectElement::CustomizableSelectEnabled(this));
    return return_value;
  }

  auto* parent_select = DynamicTo<HTMLSelectElement>(parentNode());
  if (!parent_select) {
    if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(parentNode())) {
      parent_select = DynamicTo<HTMLSelectElement>(optgroup->parentNode());
    }
  }
  if (parent_select) {
    // Don't call OptionInserted because HTMLSelectElement::ChildrenChanged or
    // HTMLOptGroupElement::ChildrenChanged will call it for us in this case. If
    // insertion_point is an ancestor of parent_select, then we shouldn't really
    // be doing anything here and OptionInserted was already called in a
    // previous insertion.
    // TODO(crbug.com/1511354): When the CustomizableSelect flag is removed, we
    // can remove the code in HTMLSelectElement::ChildrenChanged and
    // HTMLOptGroupElement::ChildrenChanged which handles this case as well as
    // the code here which avoids handling it.
    SetOwnerSelectElement(parent_select);
    return return_value;
  }

  // If there is a <select> in between this and insertion_point, then don't call
  // OptionInserted. Otherwise, if this option is being inserted into a <select>
  // ancestor, then we must call OptionInserted on it.
  bool passed_insertion_point = false;
  for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
    if (IsA<HTMLOptionElement>(ancestor)) {
      // Don't call OptionInserted() on nested <option>s. This matches the
      // traversals in OptionList and OwnerSelectElement.
      break;
    }
    if (&ancestor == &insertion_point) {
      passed_insertion_point = true;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      SetOwnerSelectElement(select);
      if (passed_insertion_point) {
        select->OptionInserted(*this, Selected());
      }
      break;
    }
  }

  return return_value;
}

void HTMLOptionElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (!HTMLSelectElement::SelectParserRelaxationEnabled(this)) {
    CHECK(!HTMLSelectElement::CustomizableSelectEnabled(this));
    return;
  }

  // This code determines the value of was_removed_from_select_parent, which
  // should be true in the case that this <option> was a child of a <select> and
  // got removed, or there was an <optgroup> directly in between this <option>
  // and a <select> and either the optgroup-option or select-optgroup child
  // relationship was disconnected.
  bool insertion_point_passed = false;
  bool is_parent_select_or_optgroup = false;
  ContainerNode* parent = parentNode();
  if (!parent) {
    parent = &insertion_point;
    insertion_point_passed = true;
  }
  if (IsA<HTMLSelectElement>(parent)) {
    is_parent_select_or_optgroup = true;
  } else if (IsA<HTMLOptGroupElement>(parent)) {
    parent = parent->parentNode();
    if (!parent) {
      parent = &insertion_point;
      insertion_point_passed = true;
    }
    is_parent_select_or_optgroup = IsA<HTMLSelectElement>(parent);
  }
  bool was_removed_from_select_parent =
      insertion_point_passed && is_parent_select_or_optgroup;

  if (was_removed_from_select_parent) {
    SetOwnerSelectElement(nullptr);
    // Don't call select->OptionRemoved() in this case because
    // HTMLSelectElement::ChildrenChanged or
    // HTMLOptGroupElement::ChildrenChanged will call it for us.
    return;
  }

  for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
    // If this option is still associated with a <select> inside the detached
    // subtree, then we should not call OptionRemoved() because we don't call
    // OptionInserted() in the corresponding attachment case. Also, APIs like
    // select.options should still work when the <select> is detached.
    // Nested options should not be associated with selects.
    if (IsA<HTMLSelectElement>(ancestor) || IsA<HTMLOptionElement>(ancestor)) {
      return;
    }
  }

  SetOwnerSelectElement(nullptr);

  for (Node& ancestor : NodeTraversal::InclusiveAncestorsOf(insertion_point)) {
    if (IsA<HTMLOptionElement>(ancestor)) {
      // Nested options should not be associated with selects.
      return;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      select->OptionRemoved(*this);
      break;
    }
  }
}

bool HTMLOptionElement::SpatialNavigationFocused() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (!select || !select->IsFocused())
    return false;
  return select->SpatialNavigationFocusedOption() == this;
}

bool HTMLOptionElement::IsDisplayNone(bool ensure_style) {
  const ComputedStyle* style = GetComputedStyle();
  if (!style && ensure_style &&
      HTMLSelectElement::SelectParserRelaxationEnabled(this)) {
    style = EnsureComputedStyle();
  }
  return !style || style->Display() == EDisplay::kNone;
}

void HTMLOptionElement::DefaultEventHandler(Event& event) {
  DefaultEventHandlerInternal(event);
  HTMLElement::DefaultEventHandler(event);
}

namespace {
bool OptionIsVisible(HTMLOptionElement& option) {
  PhysicalRect popover_rect =
      option.OwnerSelectElement()->PopoverForAppearanceBase()->BoundingBox();
  PhysicalRect option_rect = option.BoundingBox();
  LayoutUnit popover_top = popover_rect.Y();
  LayoutUnit option_top = option_rect.Y();
  return option_top >= popover_top && option_top + option_rect.Height() <=
                                          popover_top + popover_rect.Height();
}
}  // namespace

void HTMLOptionElement::DefaultEventHandlerInternal(Event& event) {
  auto* select = OwnerSelectElement();
  if (!select || !select->IsAppearanceBasePicker()) {
    // We only want to apply mouse/keyboard behavior for appearance:base-select
    // select pickers.
    return;
  }
  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (mouse_event && event.type() == event_type_names::kMouseup &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    // We leave the picker open, and do not "pick" an option, only if:
    //  1. The mousedown was on the <select> button, so we have a mousedown
    //     location stored, and
    //  2. The mouseup on this <option> was within kEpsilon layout units
    //     (post zoom, page-relative) of the location of the mousedown. I.e.
    //     the mouse was not dragged between mousedown and mouseup.
    std::optional<gfx::PointF> mouse_down_loc =
        GetDocument().CustomizableSelectMousedownLocation();
    constexpr float kEpsilon = 5;  // 5 pixels in any direction
    bool mouse_moved = !mouse_down_loc.has_value() ||
                       !mouse_down_loc->IsWithinDistance(
                           mouse_event->AbsoluteLocation(), kEpsilon);
    if (mouse_moved) {
      select->SelectOptionByPopup(this);
      select->HidePopup(SelectPopupHideBehavior::kNormal);
      event.SetDefaultHandled();
    }
    GetDocument().SetCustomizableSelectMousedownLocation(std::nullopt);
    return;
  } else if (event.type() == event_type_names::kMousedown) {
    GetDocument().SetCustomizableSelectMousedownLocation(std::nullopt);
  }

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  int tab_ignore_modifiers = WebInputEvent::kControlKey |
                             WebInputEvent::kAltKey | WebInputEvent::kMetaKey;
  int ignore_modifiers = WebInputEvent::kShiftKey | tab_ignore_modifiers;
  FocusParams focus_params(FocusTrigger::kUserGesture);

  if (keyboard_event && event.type() == event_type_names::kKeydown) {
    const AtomicString key(keyboard_event->key());
    if (!(keyboard_event->GetModifiers() & ignore_modifiers)) {
      if ((key == " " || key == keywords::kCapitalEnter)) {
        select->SelectOptionByPopup(this);
        select->HidePopup(SelectPopupHideBehavior::kNormal);
        event.SetDefaultHandled();
        return;
      }
      OptionList options = select->GetOptionList();
      if (options.Empty()) {
        // Nothing below can do anything, if the options list is empty.
        return;
      }
      if (key == keywords::kArrowUp) {
        if (auto* previous_option = options.PreviousFocusableOption(*this)) {
          previous_option->Focus(focus_params);
        }
        event.SetDefaultHandled();
        return;
      } else if (key == keywords::kArrowDown) {
        if (auto* next_option = options.NextFocusableOption(*this)) {
          next_option->Focus(focus_params);
        }
        event.SetDefaultHandled();
        return;
      } else if (key == keywords::kHome) {
        if (auto* first_option = options.NextFocusableOption(
                *options.begin(), /*inclusive*/ true)) {
          first_option->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      } else if (key == keywords::kEnd) {
        if (auto* last_option = options.PreviousFocusableOption(
                *options.last(), /*inclusive*/ true)) {
          last_option->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      } else if (key == keywords::kPageDown) {
        if (!OptionIsVisible(*this)) {
          // If the option isn't visible at all right now, *only* scroll it into
          // view.
          scrollIntoViewIfNeeded(/*center_if_needed*/ false);
        } else {
          auto* next_option = options.NextFocusableOption(*this);
          if (next_option && !OptionIsVisible(*next_option)) {
            // The next option isn't visible, which means we were at the very
            // bottom. Scroll the current option to the top, and then focus the
            // bottom one.
            scrollIntoView(/*align_to_top*/ true);
          }
          // Then find the last option that is still in the view.
          HTMLOptionElement* next_focus = this;
          for (auto* current = this; current && OptionIsVisible(*current);
               current = options.NextFocusableOption(*current)) {
            next_focus = current;
          }
          next_focus->Focus(focus_params);
        }
        event.SetDefaultHandled();
      } else if (key == keywords::kPageUp) {
        if (!OptionIsVisible(*this)) {
          // If the option isn't visible at all right now, *only* scroll it into
          // view.
          scrollIntoViewIfNeeded(/*center_if_needed*/ false);
        } else {
          auto* previous_option = options.PreviousFocusableOption(*this);
          if (previous_option && !OptionIsVisible(*previous_option)) {
            // The previous option isn't visible, which means we were at the
            // very top. Scroll the current option to the bottom, and then focus
            // the top one.
            scrollIntoView(/*align_to_top*/ false);
          }
          // Then find the first option that is in the view.
          HTMLOptionElement* next_focus = this;
          for (auto* current = this; current && OptionIsVisible(*current);
               current = options.PreviousFocusableOption(*current)) {
            next_focus = current;
          }
          next_focus->Focus(focus_params);
        }
        event.SetDefaultHandled();
      }
    }

    if (key == keywords::kTab &&
        !(keyboard_event->GetModifiers() & tab_ignore_modifiers) &&
        !select->IsInDialogMode()) {
      // TODO(http://crbug.com/1511354): Consider focusing something in this
      // case. https://github.com/openui/open-ui/issues/1016
      select->HidePopup(SelectPopupHideBehavior::kNormal);
      event.SetDefaultHandled();
      return;
    }
  }
}

void HTMLOptionElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
  if (HTMLSelectElement::CustomizableSelectEnabled(this) && Selected()) {
    auto* select = OwnerSelectElement();
    if (select && !select->IsMultiple()) {
      select->UpdateAllSelectedcontents(this);
    }
  }
}

// static
bool HTMLOptionElement::IsLabelContainerElement(const Element& element) {
  if (!HTMLSelectElement::CustomizableSelectEnabled(&element)) {
    return false;
  }
  return IsA<HTMLOptionElement>(element.OwnerShadowHost()) &&
         element.ShadowPseudoId() ==
             shadow_element_names::kOptionLabelContainer;
}

}  // namespace blink
