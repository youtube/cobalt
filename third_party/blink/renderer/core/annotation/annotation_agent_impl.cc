// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

AnnotationAgentImpl::AnnotationAgentImpl(
    AnnotationAgentContainerImpl& owning_container,
    mojom::blink::AnnotationType annotation_type,
    AnnotationSelector& selector,
    AnnotationAgentContainerImpl::PassKey)
    : agent_host_(owning_container.GetSupplementable()->GetExecutionContext()),
      receiver_(this,
                owning_container.GetSupplementable()->GetExecutionContext()),
      owning_container_(&owning_container),
      selector_(&selector),
      type_(annotation_type) {
  DCHECK(!IsAttached());
  DCHECK(!IsRemoved());
}

void AnnotationAgentImpl::Trace(Visitor* visitor) const {
  visitor->Trace(agent_host_);
  visitor->Trace(receiver_);
  visitor->Trace(owning_container_);
  visitor->Trace(selector_);
  visitor->Trace(attached_range_);
}

void AnnotationAgentImpl::Bind(
    mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver) {
  DCHECK(!IsRemoved());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      owning_container_->GetSupplementable()->GetTaskRunner(
          TaskType::kInternalDefault);

  agent_host_.Bind(std::move(host_remote), task_runner);
  receiver_.Bind(std::move(agent_receiver), task_runner);

  // Breaking the mojo connection will cause this agent to remove itself from
  // the container.
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&AnnotationAgentImpl::Remove, WrapWeakPersistent(this)));
}

void AnnotationAgentImpl::Attach() {
  TRACE_EVENT("blink", "AnnotationAgentImpl::Attach");
  DCHECK(!IsRemoved());
  did_try_attach_ = true;
  Document& document = *owning_container_->GetSupplementable();
  selector_->FindRange(document, AnnotationSelector::kSynchronous,
                       WTF::BindOnce(&AnnotationAgentImpl::DidFinishAttach,
                                     WrapWeakPersistent(this)));
}

bool AnnotationAgentImpl::IsAttached() const {
  // An attached range may have !IsCollapsed but converting to EphemeralRange
  // results in IsCollapsed. For an example, see
  // AnnotationAgentImplTest.ScrollIntoViewCollapsedRange.
  return attached_range_ && attached_range_->IsConnected() &&
         !attached_range_->IsCollapsed() &&
         !attached_range_->ToEphemeralRange().IsCollapsed();
}

bool AnnotationAgentImpl::IsBoundForTesting() const {
  DCHECK_EQ(agent_host_.is_bound(), receiver_.is_bound());
  return receiver_.is_bound();
}

void AnnotationAgentImpl::Remove() {
  DCHECK(!IsRemoved());

  if (IsAttached()) {
    EphemeralRange dom_range =
        EphemeralRange(ToPositionInDOMTree(attached_range_->StartPosition()),
                       ToPositionInDOMTree(attached_range_->EndPosition()));
    Document* document = attached_range_->StartPosition().GetDocument();
    DCHECK(document);

    if (LocalFrame* frame = document->GetFrame()) {
      // Markers require that layout is up to date if we're making any
      // modifications.
      frame->GetDocument()->UpdateStyleAndLayout(
          DocumentUpdateReason::kFindInPage);

      // TODO(bokan): Base marker type on `type_`.
      document->Markers().RemoveMarkersInRange(
          dom_range, DocumentMarker::MarkerTypes::TextFragment());
    }

    attached_range_.Clear();
  }

  agent_host_.reset();
  receiver_.reset();
  owning_container_->RemoveAgent(*this, PassKey());

  selector_.Clear();
  owning_container_.Clear();
}

void AnnotationAgentImpl::ScrollIntoView() const {
  DCHECK(!IsRemoved());

  if (!IsAttached())
    return;

  EphemeralRangeInFlatTree range = attached_range_->ToEphemeralRange();
  CHECK(range.Nodes().begin() != range.Nodes().end());
  Node& first_node = *range.Nodes().begin();

  Document& document = *owning_container_->GetSupplementable();
  document.EnsurePaintLocationDataValidForNode(
      &first_node, DocumentUpdateReason::kFindInPage);

  // TODO(bokan): Text can be attached without having a LayoutObject since it
  // may be inside an unexpanded <details> element or inside a
  // `content-visibility: auto` subtree. In those cases we should make sure we
  // expand/make-visible the node. This is implemented in TextFragmentAnchor
  // but that doesn't cover all cases we can get here so we should migrate that
  // code here.
  if (!first_node.GetLayoutObject()) {
    return;
  }

  // Set the bounding box height to zero because we want to center the top of
  // the text range.
  PhysicalRect bounding_box(ComputeTextRect(range));
  bounding_box.SetHeight(LayoutUnit());

  mojom::blink::ScrollIntoViewParamsPtr params =
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::CenterAlways(), ScrollAlignment::CenterAlways(),
          mojom::blink::ScrollType::kProgrammatic);
  params->cross_origin_boundaries = false;

  scroll_into_view_util::ScrollRectToVisible(*first_node.GetLayoutObject(),
                                             bounding_box, std::move(params));
}

void AnnotationAgentImpl::DidFinishAttach(const RangeInFlatTree* range) {
  TRACE_EVENT("blink", "AnnotationAgentImpl::DidFinishAttach", "bound_to_host",
              agent_host_.is_bound());
  if (IsRemoved()) {
    TRACE_EVENT_INSTANT("blink", "Removed");
    return;
  }

  attached_range_ = range;

  if (IsAttached()) {
    TRACE_EVENT_INSTANT("blink", "IsAttached");
    EphemeralRange dom_range =
        EphemeralRange(ToPositionInDOMTree(attached_range_->StartPosition()),
                       ToPositionInDOMTree(attached_range_->EndPosition()));
    Document* document = attached_range_->StartPosition().GetDocument();
    DCHECK(document);

    // TODO(bokan): DocumentMarkers don't support overlapping markers. We could
    // be smarter about how we construct markers so they don't overlap - or we
    // could make DocumentMarkerController allow overlaps.
    // https://crbug.com/1327370.
    if (document->Markers()
            .MarkersIntersectingRange(
                attached_range_->ToEphemeralRange(),
                DocumentMarker::MarkerTypes::TextFragment())
            .empty()) {
      // TODO(bokan): Add new marker types based on `type_`.
      document->Markers().AddTextFragmentMarker(dom_range);
    } else {
      TRACE_EVENT_INSTANT("blink", "Markers Intersect!");
    }
  } else {
    TRACE_EVENT_INSTANT("blink", "NotAttached");
    attached_range_.Clear();
  }

  // If we're bound to one, let the host know we've finished attempting to
  // attach.
  // TODO(bokan): Perhaps we should keep track of whether we've called
  // DidFinishAttach and, if set, call the host method when binding.
  if (agent_host_.is_bound()) {
    gfx::Rect range_rect_in_document;
    if (IsAttached()) {
      gfx::Rect rect_in_frame =
          ComputeTextRect(attached_range_->ToEphemeralRange());

      Document* document = attached_range_->StartPosition().GetDocument();
      DCHECK(document);

      LocalFrameView* view = document->View();
      DCHECK(view);

      range_rect_in_document = view->FrameToDocument(rect_in_frame);
    }

    // Empty rect means the selector didn't find its content.
    agent_host_->DidFinishAttachment(range_rect_in_document);
  }
}

bool AnnotationAgentImpl::IsRemoved() const {
  // selector_ and owning_container_ should only ever be null if the agent was
  // removed.
  DCHECK_EQ(!owning_container_, !selector_);

  // If the agent is removed, all its state should be cleared.
  DCHECK(owning_container_ || !attached_range_);
  DCHECK(owning_container_ || !agent_host_.is_bound());
  DCHECK(owning_container_ || !receiver_.is_bound());
  return !owning_container_;
}

}  // namespace blink
