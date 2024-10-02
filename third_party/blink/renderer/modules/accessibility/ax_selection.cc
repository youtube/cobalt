// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

namespace {

// TODO(nektar): Add Web tests for this event.
void ScheduleSelectEvent(TextControlElement& text_control) {
  Event* event = Event::CreateBubble(event_type_names::kSelect);
  event->SetTarget(&text_control);
  text_control.GetDocument().EnqueueAnimationFrameEvent(event);
}

// TODO(nektar): Add Web tests for this event.
DispatchEventResult DispatchSelectStart(Node* node) {
  if (!node)
    return DispatchEventResult::kNotCanceled;

  return node->DispatchEvent(
      *Event::CreateCancelableBubble(event_type_names::kSelectstart));
}

}  // namespace

//
// AXSelection::Builder
//

AXSelection::Builder& AXSelection::Builder::SetBase(const AXPosition& base) {
  DCHECK(base.IsValid());
  selection_.base_ = base;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetBase(const Position& base) {
  const auto ax_base = AXPosition::FromPosition(base);
  DCHECK(ax_base.IsValid());
  selection_.base_ = ax_base;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetExtent(
    const AXPosition& extent) {
  DCHECK(extent.IsValid());
  selection_.extent_ = extent;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetExtent(const Position& extent) {
  const auto ax_extent = AXPosition::FromPosition(extent);
  DCHECK(ax_extent.IsValid());
  selection_.extent_ = ax_extent;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetSelection(
    const SelectionInDOMTree& selection) {
  if (selection.IsNone())
    return *this;

  selection_.base_ = AXPosition::FromPosition(selection.Base());
  selection_.extent_ = AXPosition::FromPosition(selection.Extent());
  return *this;
}

const AXSelection AXSelection::Builder::Build() {
  if (!selection_.Base().IsValid() || !selection_.Extent().IsValid())
    return {};

  const Document* document = selection_.Base().ContainerObject()->GetDocument();
  DCHECK(document);
  DCHECK(document->IsActive());
  DCHECK(!document->NeedsLayoutTreeUpdate());
  // We don't support selections that span across documents.
  if (selection_.Extent().ContainerObject()->GetDocument() != document)
    return {};

#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = document->DomTreeVersion();
  selection_.style_version_ = document->StyleVersion();
#endif
  return selection_;
}

//
// AXSelection
//

// static
void AXSelection::ClearCurrentSelection(Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return;

  frame_selection.Clear();
}

// static
AXSelection AXSelection::FromCurrentSelection(
    const Document& document,
    const AXSelectionBehavior selection_behavior) {
  const LocalFrame* frame = document.GetFrame();
  if (!frame)
    return {};

  const FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return {};

  return FromSelection(frame_selection.GetSelectionInDOMTree(),
                       selection_behavior);
}

// static
AXSelection AXSelection::FromCurrentSelection(
    const TextControlElement& text_control) {
  const Document& document = text_control.GetDocument();
  AXObjectCache* ax_object_cache = document.ExistingAXObjectCache();
  if (!ax_object_cache)
    return {};

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  const AXObject* ax_text_control =
      ax_object_cache_impl->GetOrCreate(&text_control);
  DCHECK(ax_text_control);

  // We can't directly use "text_control.Selection()" because the selection it
  // returns is inside the shadow DOM and it's not anchored to the text field
  // itself.
  const TextAffinity extent_affinity = text_control.Selection().Affinity();
  const TextAffinity base_affinity =
      text_control.selectionStart() == text_control.selectionEnd()
          ? extent_affinity
          : TextAffinity::kDownstream;

  const bool is_backward = (text_control.selectionDirection() == "backward");
  const auto ax_base = AXPosition::CreatePositionInTextObject(
      *ax_text_control,
      static_cast<int>(is_backward ? text_control.selectionEnd()
                                   : text_control.selectionStart()),
      base_affinity);
  const auto ax_extent = AXPosition::CreatePositionInTextObject(
      *ax_text_control,
      static_cast<int>(is_backward ? text_control.selectionStart()
                                   : text_control.selectionEnd()),
      extent_affinity);

  if (!ax_base.IsValid() || !ax_extent.IsValid())
    return {};

  AXSelection::Builder selection_builder;
  selection_builder.SetBase(ax_base).SetExtent(ax_extent);
  return selection_builder.Build();
}

// static
AXSelection AXSelection::FromSelection(
    const SelectionInDOMTree& selection,
    const AXSelectionBehavior selection_behavior) {
  if (selection.IsNone())
    return {};
  DCHECK(selection.AssertValid());

  const Position dom_base = selection.Base();
  const Position dom_extent = selection.Extent();
  const TextAffinity extent_affinity = selection.Affinity();
  const TextAffinity base_affinity =
      selection.IsCaret() ? extent_affinity : TextAffinity::kDownstream;

  AXPositionAdjustmentBehavior base_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  AXPositionAdjustmentBehavior extent_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  // If the selection is not collapsed, extend or shrink the DOM selection if
  // there is no equivalent selection in the accessibility tree, i.e. if the
  // corresponding endpoints are either ignored or unavailable in the
  // accessibility tree. If the selection is collapsed, move both endpoints to
  // the next valid position in the accessibility tree but do not extend or
  // shrink the selection, because this will result in a non-collapsed selection
  // in the accessibility tree.
  if (!selection.IsCaret()) {
    switch (selection_behavior) {
      case AXSelectionBehavior::kShrinkToValidRange:
        if (selection.IsBaseFirst()) {
          base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
          extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        } else {
          base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
          extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        }
        break;
      case AXSelectionBehavior::kExtendToValidRange:
        if (selection.IsBaseFirst()) {
          base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
          extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        } else {
          base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
          extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        }
        break;
    }
  }

  const auto ax_base =
      AXPosition::FromPosition(dom_base, base_affinity, base_adjustment);
  const auto ax_extent =
      AXPosition::FromPosition(dom_extent, extent_affinity, extent_adjustment);

  if (!ax_base.IsValid() || !ax_extent.IsValid())
    return {};

  AXSelection::Builder selection_builder;
  selection_builder.SetBase(ax_base).SetExtent(ax_extent);
  return selection_builder.Build();
}

AXSelection::AXSelection() : base_(), extent_() {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

bool AXSelection::IsValid() const {
  if (!base_.IsValid() || !extent_.IsValid())
    return false;

  // We don't support selections that span across documents.
  if (base_.ContainerObject()->GetDocument() !=
      extent_.ContainerObject()->GetDocument()) {
    return false;
  }

  //
  // The following code checks if a text position in a text control is valid.
  // Since the contents of a text control are implemented using user agent
  // shadow DOM, we want to prevent users from selecting across the shadow DOM
  // boundary.
  //
  // TODO(nektar): Generalize this logic to adjust user selection if it crosses
  // disallowed shadow DOM boundaries such as user agent shadow DOM, editing
  // boundaries, replaced elements, CSS user-select, etc.
  //

  if (base_.IsTextPosition() && base_.ContainerObject()->IsAtomicTextField() &&
      !(base_.ContainerObject() == extent_.ContainerObject() &&
        extent_.IsTextPosition() &&
        extent_.ContainerObject()->IsAtomicTextField())) {
    return false;
  }

  if (extent_.IsTextPosition() &&
      extent_.ContainerObject()->IsAtomicTextField() &&
      !(base_.ContainerObject() == extent_.ContainerObject() &&
        base_.IsTextPosition() &&
        base_.ContainerObject()->IsAtomicTextField())) {
    return false;
  }

  DCHECK(!base_.ContainerObject()->GetDocument()->NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(base_.ContainerObject()->GetDocument()->DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(base_.ContainerObject()->GetDocument()->StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

const SelectionInDOMTree AXSelection::AsSelection(
    const AXSelectionBehavior selection_behavior) const {
  if (!IsValid())
    return {};

  AXPositionAdjustmentBehavior base_adjustment =
      AXPositionAdjustmentBehavior::kMoveLeft;
  AXPositionAdjustmentBehavior extent_adjustment =
      AXPositionAdjustmentBehavior::kMoveLeft;
  switch (selection_behavior) {
    case AXSelectionBehavior::kShrinkToValidRange:
      if (base_ < extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      } else if (base_ > extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      }
      break;
    case AXSelectionBehavior::kExtendToValidRange:
      if (base_ < extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      } else if (base_ > extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      }
      break;
  }

  const auto dom_base = base_.ToPositionWithAffinity(base_adjustment);
  const auto dom_extent = extent_.ToPositionWithAffinity(extent_adjustment);
  SelectionInDOMTree::Builder selection_builder;
  selection_builder.SetBaseAndExtent(dom_base.GetPosition(),
                                     dom_extent.GetPosition());
  if (extent_.IsTextPosition())
    selection_builder.SetAffinity(extent_.Affinity());
  return selection_builder.Build();
}

void AXSelection::UpdateSelectionIfNecessary() {
  Document* document = base_.ContainerObject()->GetDocument();
  if (!document)
    return;

  LocalFrameView* view = document->View();
  if (!view || !view->LayoutPending())
    return;

  document->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
#if DCHECK_IS_ON()
  base_.dom_tree_version_ = extent_.dom_tree_version_ = dom_tree_version_ =
      document->DomTreeVersion();
  base_.style_version_ = extent_.style_version_ = style_version_ =
      document->StyleVersion();
#endif  // DCHECK_IS_ON()
}

bool AXSelection::Select(const AXSelectionBehavior selection_behavior) {
  if (!IsValid()) {
    NOTREACHED() << "Trying to select an invalid accessibility selection.";
    return false;
  }

  absl::optional<AXSelection::TextControlSelection> text_control_selection =
      AsTextControlSelection();
  if (text_control_selection.has_value()) {
    DCHECK_LE(text_control_selection->start, text_control_selection->end);
    TextControlElement& text_control = ToTextControl(
        *base_.ContainerObject()->GetAtomicTextFieldAncestor()->GetNode());
    if (!text_control.SetSelectionRange(text_control_selection->start,
                                        text_control_selection->end,
                                        text_control_selection->direction)) {
      return false;
    }

    // TextControl::SetSelectionRange deliberately does not set focus. But if
    // we're updating the selection, the text control should be focused.
    ScheduleSelectEvent(text_control);
    text_control.Focus();
    return true;
  }

  const SelectionInDOMTree old_selection = AsSelection(selection_behavior);
  DCHECK(old_selection.AssertValid());
  Document* document = old_selection.Base().GetDocument();
  if (!document) {
    NOTREACHED() << "Valid DOM selections should have an attached document.";
    return false;
  }

  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    NOTREACHED();
    return false;
  }

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return false;

  // See the following section in the Selection API Specification:
  // https://w3c.github.io/selection-api/#selectstart-event
  if (DispatchSelectStart(old_selection.Base().ComputeContainerNode()) !=
      DispatchEventResult::kNotCanceled) {
    return false;
  }

  UpdateSelectionIfNecessary();
  if (!IsValid())
    return false;

  // Dispatching the "selectstart" event could potentially change the document
  // associated with the current frame.
  if (!frame_selection.IsAvailable())
    return false;

  // Re-retrieve the SelectionInDOMTree in case a DOM mutation took place.
  // That way it will also have the updated DOM tree and Style versions,
  // and the SelectionTemplate checks for each won't fail.
  const SelectionInDOMTree selection = AsSelection(selection_behavior);

  SetSelectionOptions::Builder options_builder;
  options_builder.SetIsDirectional(true)
      .SetShouldCloseTyping(true)
      .SetShouldClearTypingStyle(true)
      .SetSetSelectionBy(SetSelectionBy::kUser);
  frame_selection.SetSelectionForAccessibility(selection,
                                               options_builder.Build());
  return true;
}

String AXSelection::ToString() const {
  if (!IsValid())
    return "Invalid AXSelection";
  return "AXSelection from " + Base().ToString() + " to " + Extent().ToString();
}

absl::optional<AXSelection::TextControlSelection>
AXSelection::AsTextControlSelection() const {
  if (!IsValid() || !base_.IsTextPosition() || !extent_.IsTextPosition() ||
      base_.ContainerObject() != extent_.ContainerObject()) {
    return {};
  }

  const AXObject* text_control =
      base_.ContainerObject()->GetAtomicTextFieldAncestor();
  if (!text_control)
    return {};

  DCHECK(IsTextControl(text_control->GetNode()));

  if (base_ <= extent_) {
    return TextControlSelection(base_.TextOffset(), extent_.TextOffset(),
                                kSelectionHasForwardDirection);
  }
  return TextControlSelection(extent_.TextOffset(), base_.TextOffset(),
                              kSelectionHasBackwardDirection);
}

bool operator==(const AXSelection& a, const AXSelection& b) {
  DCHECK(a.IsValid() && b.IsValid());
  return a.Base() == b.Base() && a.Extent() == b.Extent();
}

bool operator!=(const AXSelection& a, const AXSelection& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& ostream, const AXSelection& selection) {
  return ostream << selection.ToString().Utf8();
}

}  // namespace blink
