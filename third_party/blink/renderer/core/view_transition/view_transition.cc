// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

}  // namespace

ViewTransition::ScopedPauseRendering::ScopedPauseRendering(
    const Document& document) {
  if (!document.GetFrame()->IsLocalRoot())
    return;

  auto& client = document.GetPage()->GetChromeClient();
  cc_paused_ = client.PauseRendering(*document.GetFrame());
  DCHECK(cc_paused_);
}

ViewTransition::ScopedPauseRendering::~ScopedPauseRendering() = default;

bool ViewTransition::ScopedPauseRendering::ShouldThrottleRendering() const {
  return !cc_paused_;
}

void ViewTransition::UpdateSnapshotContainingBlockStyle() {
  LayoutViewTransitionRoot* transition_root =
      document_->GetLayoutView()->GetViewTransitionRoot();
  CHECK(transition_root);
  transition_root->UpdateSnapshotStyle(*style_tracker_);
}

// static
const char* ViewTransition::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kCaptureTagDiscovery:
      return "CaptureTagDiscovery";
    case State::kCaptureRequestPending:
      return "CaptureRequestPending";
    case State::kCapturing:
      return "Capturing";
    case State::kCaptured:
      return "Captured";
    case State::kWaitForRenderBlock:
      return "WaitForRenderBlock";
    case State::kDOMCallbackRunning:
      return "DOMCallbackRunning";
    case State::kDOMCallbackFinished:
      return "DOMCallbackFinished";
    case State::kAnimateTagDiscovery:
      return "AnimateTagDiscovery";
    case State::kAnimateRequestPending:
      return "AnimateRequestPending";
    case State::kAnimating:
      return "Animating";
    case State::kFinished:
      return "Finished";
    case State::kAborted:
      return "Aborted";
    case State::kTimedOut:
      return "TimedOut";
    case State::kTransitionStateCallbackDispatched:
      return "TransitionStateCallbackDispatched";
  };
  NOTREACHED();
  return "";
}

// static
ViewTransition* ViewTransition::CreateFromScript(
    Document* document,
    V8ViewTransitionCallback* callback,
    Delegate* delegate) {
  CHECK(document->GetExecutionContext());
  return MakeGarbageCollected<ViewTransition>(PassKey(), document, callback,
                                              delegate);
}

ViewTransition::ViewTransition(PassKey,
                               Document* document,
                               V8ViewTransitionCallback* update_dom_callback,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kScript),
      document_(document),
      delegate_(delegate),
      document_tag_(NextDocumentTag()),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*document_)),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *document->GetExecutionContext(),
          *this,
          update_dom_callback)) {
  ProcessCurrentState();
}

// static
ViewTransition* ViewTransition::CreateForSnapshotForNavigation(
    Document* document,
    ViewTransitionStateCallback callback,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(PassKey(), document,
                                              std::move(callback), delegate);
}

ViewTransition::ViewTransition(PassKey,
                               Document* document,
                               ViewTransitionStateCallback callback,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kForSnapshot),
      document_(document),
      delegate_(delegate),
      navigation_id_(viz::NavigationID::Create()),
      document_tag_(NextDocumentTag()),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*document_)),
      transition_state_callback_(std::move(callback)) {
  TRACE_EVENT0("blink", "ViewTransition::ViewTransition - CreatedForSnapshot");
  DCHECK(transition_state_callback_);
  ProcessCurrentState();
}

// static
ViewTransition* ViewTransition::CreateFromSnapshotForNavigation(
    Document* document,
    ViewTransitionState transition_state,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(
      PassKey(), document, std::move(transition_state), delegate);
}

ViewTransition::ViewTransition(PassKey,
                               Document* document,
                               ViewTransitionState transition_state,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kFromSnapshot),
      document_(document),
      delegate_(delegate),
      navigation_id_(transition_state.navigation_id),
      document_tag_(NextDocumentTag()),
      style_tracker_(MakeGarbageCollected<ViewTransitionStyleTracker>(
          *document_,
          std::move(transition_state))),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *document_->GetExecutionContext(),
          *this)) {
  TRACE_EVENT0("blink",
               "ViewTransition::ViewTransition - CreatingFromSnapshot");
  bool process_next_state = AdvanceTo(State::kWaitForRenderBlock);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::SkipTransition(PromiseResponse response) {
  DCHECK_NE(response, PromiseResponse::kResolve);
  if (IsTerminalState(state_))
    return;

  // Cleanup logic which is tied to ViewTransition objects created using the
  // script API. script_delegate_ is cleared when the Document is being torn
  // down and script specific callbacks don't need to be dispatched in that
  // case.
  if (script_delegate_) {
    CHECK_NE(creation_type_, CreationType::kForSnapshot);
    script_delegate_->DidSkipTransition(response);
  }

  // If we already started processing the transition (i.e. we're beyond capture
  // tag discovery), then send a release directive.
  if (static_cast<int>(state_) >
      static_cast<int>(State::kCaptureTagDiscovery)) {
    delegate_->AddPendingRequest(
        ViewTransitionRequest::CreateRelease(document_tag_, navigation_id_));
  }

  // We always need to call the transition state callback (mojo seems to require
  // this contract), so do so if we have one and we haven't called it yet.
  if (transition_state_callback_) {
    CHECK_EQ(creation_type_, CreationType::kForSnapshot);
    ViewTransitionState view_transition_state;
    view_transition_state.navigation_id = navigation_id_;
    std::move(transition_state_callback_).Run(std::move(view_transition_state));
  }

  // Resume rendering, and finalize the rest of the state.
  ResumeRendering();
  style_tracker_->Abort();

  delegate_->OnTransitionFinished(this);

  // This should be the last call in this function to avoid erroneously checking
  // the `state_` against the wrong state.
  AdvanceTo(State::kAborted);
}

bool ViewTransition::AdvanceTo(State state) {
  DCHECK(CanAdvanceTo(state)) << "Current state " << static_cast<int>(state_)
                              << " new state " << static_cast<int>(state);
  state_ = state;

  // If we need to run in a lifecycle, but we're not in one, then make sure to
  // schedule an animation in case we wouldn't get one naturally.
  if (StateRunsInViewTransitionStepsDuringMainFrame(state_) !=
      in_main_lifecycle_update_) {
    if (!in_main_lifecycle_update_) {
      DCHECK(!IsTerminalState(state_));
      document_->View()->ScheduleAnimation();
    } else {
      DCHECK(IsTerminalState(state_) || WaitsForNotification(state_));
    }
    return false;
  }
  // In all other cases, we should be able to process the state immediately. We
  // don't do it in this function so that it's clear what's happening outside of
  // this call.
  return true;
}

bool ViewTransition::CanAdvanceTo(State state) const {
  // This documents valid state transitions. Note that this does not make a
  // judgement call about whether the state runs synchronously or not,
  // so we allow some transitions that would not be possible in a synchronous
  // run, like kCaptured -> kAborted. This isn't possible in a synchronous call,
  // because kCaptured will always go to kDOMCallbackRunning.

  switch (state_) {
    case State::kInitial:
      return state == State::kCaptureTagDiscovery ||
             state == State::kWaitForRenderBlock;
    case State::kCaptureTagDiscovery:
      return state == State::kCaptureRequestPending || state == State::kAborted;
    case State::kCaptureRequestPending:
      return state == State::kCapturing || state == State::kAborted;
    case State::kCapturing:
      return state == State::kCaptured || state == State::kAborted;
    case State::kCaptured:
      return state == State::kDOMCallbackRunning ||
             state == State::kDOMCallbackFinished || state == State::kAborted ||
             state == State::kTransitionStateCallbackDispatched;
    case State::kTransitionStateCallbackDispatched:
      // This transition must finish on a ViewTransition bound to the new
      // Document.
      return state == State::kAborted;
    case State::kWaitForRenderBlock:
      return state == State::kAnimateTagDiscovery || state == State::kAborted;
    case State::kDOMCallbackRunning:
      return state == State::kDOMCallbackFinished || state == State::kAborted;
    case State::kDOMCallbackFinished:
      return state == State::kAnimateTagDiscovery || state == State::kAborted;
    case State::kAnimateTagDiscovery:
      return state == State::kAnimateRequestPending || state == State::kAborted;
    case State::kAnimateRequestPending:
      return state == State::kAnimating || state == State::kAborted;
    case State::kAnimating:
      return state == State::kFinished || state == State::kAborted;
    case State::kAborted:
      // We allow aborted to move to timed out state, so that time out can call
      // skipTransition and then change the state to timed out.
      return state == State::kTimedOut;
    case State::kFinished:
    case State::kTimedOut:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool ViewTransition::StateRunsInViewTransitionStepsDuringMainFrame(
    State state) {
  switch (state) {
    case State::kInitial:
      return false;
    case State::kCaptureTagDiscovery:
    case State::kCaptureRequestPending:
      return true;
    case State::kCapturing:
    case State::kCaptured:
    case State::kWaitForRenderBlock:
    case State::kDOMCallbackRunning:
    case State::kDOMCallbackFinished:
    case State::kAnimateTagDiscovery:
    case State::kAnimateRequestPending:
      return false;
    case State::kAnimating:
      return true;
    case State::kFinished:
    case State::kAborted:
    case State::kTimedOut:
    case State::kTransitionStateCallbackDispatched:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool ViewTransition::WaitsForNotification(State state) {
  return state == State::kCapturing || state == State::kDOMCallbackRunning ||
         state == State::kWaitForRenderBlock ||
         state == State::kTransitionStateCallbackDispatched;
}

// static
bool ViewTransition::IsTerminalState(State state) {
  return state == State::kFinished || state == State::kAborted ||
         state == State::kTimedOut;
}

void ViewTransition::ProcessCurrentState() {
  bool process_next_state = true;
  while (process_next_state) {
    DCHECK_EQ(in_main_lifecycle_update_,
              StateRunsInViewTransitionStepsDuringMainFrame(state_));
    TRACE_EVENT1("blink", "ViewTransition::ProcessCurrentState", "state",
                 StateToString(state_));
    process_next_state = false;
    switch (state_) {
      // Initial state: nothing to do, just advance the state
      case State::kInitial:
        // We require a new effect node to be generated for the LayoutView when
        // a transition is not in terminal state. Dirty paint to ensure
        // generation of this effect node.
        if (auto* layout_view = document_->GetLayoutView()) {
          layout_view->SetNeedsPaintPropertyUpdate();
        }

        process_next_state = AdvanceTo(State::kCaptureTagDiscovery);
        DCHECK(!process_next_state);
        break;

      // Update the lifecycle if needed and discover the elements (deferred to
      // AddTransitionElementsFromCSS).
      case State::kCaptureTagDiscovery:
        DCHECK(in_main_lifecycle_update_);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kCompositingInputsClean);
        style_tracker_->AddTransitionElementsFromCSS();
        process_next_state = AdvanceTo(State::kCaptureRequestPending);
        DCHECK(process_next_state);
        break;

      // Capture request pending -- create the request
      case State::kCaptureRequestPending:
        if (!style_tracker_->Capture()) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        delegate_->AddPendingRequest(ViewTransitionRequest::CreateCapture(
            document_tag_, style_tracker_->CapturedTagCount(), navigation_id_,
            style_tracker_->TakeCaptureResourceIds(),
            ConvertToBaseOnceCallback(
                CrossThreadBindOnce(&ViewTransition::NotifyCaptureFinished,
                                    MakeUnwrappingCrossThreadHandle(this)))));

        if (document_->GetFrame()->IsLocalRoot()) {
          // We need to ensure commits aren't deferred since we rely on commits
          // to send directives to the compositor and initiate pause of
          // rendering after one frame.
          document_->GetPage()->GetChromeClient().StopDeferringCommits(
              *document_->GetFrame(),
              cc::PaintHoldingCommitTrigger::kViewTransition);
        }
        document_->GetPage()->GetChromeClient().RegisterForCommitObservation(
            this);

        process_next_state = AdvanceTo(State::kCapturing);
        DCHECK(!process_next_state);
        break;

      case State::kCapturing:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kCaptured: {
        style_tracker_->CaptureResolved();

        if (creation_type_ == CreationType::kForSnapshot) {
          DCHECK(transition_state_callback_);
          ViewTransitionState view_transition_state =
              style_tracker_->GetViewTransitionState();
          view_transition_state.navigation_id = navigation_id_;

          process_next_state =
              AdvanceTo(State::kTransitionStateCallbackDispatched);
          DCHECK(process_next_state);

          std::move(transition_state_callback_)
              .Run(std::move(view_transition_state));
          break;
        }

        // The following logic is only executed for ViewTransition objects
        // created by the script API.
        CHECK_EQ(creation_type_, CreationType::kScript);
        CHECK(script_delegate_);
        script_delegate_->InvokeDOMChangeCallback();

        // Since invoking the callback could yield (at least when devtools
        // breakpoint is hit, but maybe in other situations), we could have
        // timed out already. Make sure we don't advance the state out of a
        // terminal state.
        if (IsTerminalState(state_)) {
          break;
        }

        process_next_state = AdvanceTo(State::kDOMCallbackRunning);
        DCHECK(process_next_state);
        break;
      }

      case State::kWaitForRenderBlock:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kDOMCallbackRunning:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kDOMCallbackFinished:
        // For testing check: if the flag is enabled, re-create the style
        // tracker with the serialized state that the current style tracker
        // produces. This allows us to use SPA tests for MPA serialization.
        if (RuntimeEnabledFeatures::
                SerializeViewTransitionStateInSPAEnabled()) {
          style_tracker_ = MakeGarbageCollected<ViewTransitionStyleTracker>(
              *document_, style_tracker_->GetViewTransitionState());
        }

        ResumeRendering();
        process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
        DCHECK(process_next_state);
        break;

      case State::kAnimateTagDiscovery:
        DCHECK(!in_main_lifecycle_update_);
        document_->View()->UpdateLifecycleToPrePaintClean(
            DocumentUpdateReason::kViewTransition);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kPrePaintClean);
        style_tracker_->AddTransitionElementsFromCSS();
        process_next_state = AdvanceTo(State::kAnimateRequestPending);
        DCHECK(process_next_state);
        break;

      case State::kAnimateRequestPending:
        if (!style_tracker_->Start()) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        delegate_->AddPendingRequest(
            ViewTransitionRequest::CreateAnimateRenderer(document_tag_,
                                                         navigation_id_));
        process_next_state = AdvanceTo(State::kAnimating);
        DCHECK(!process_next_state);

        DCHECK(!in_main_lifecycle_update_);
        CHECK_NE(creation_type_, CreationType::kForSnapshot);
        CHECK(script_delegate_);
        script_delegate_->DidStartAnimating();
        break;

      case State::kAnimating: {
        if (first_animating_frame_) {
          first_animating_frame_ = false;
          // We need to schedule an animation frame, in case this is the only
          // kAnimating frame we will get, so that we can clean up in the next
          // frame.
          document_->View()->ScheduleAnimation();
          break;
        }

        if (style_tracker_->HasActiveAnimations())
          break;

        style_tracker_->StartFinished();

        CHECK_NE(creation_type_, CreationType::kForSnapshot);
        CHECK(script_delegate_);
        script_delegate_->DidFinishAnimating();

        delegate_->AddPendingRequest(ViewTransitionRequest::CreateRelease(
            document_tag_, navigation_id_));
        delegate_->OnTransitionFinished(this);

        style_tracker_ = nullptr;
        process_next_state = AdvanceTo(State::kFinished);
        DCHECK(!process_next_state);
        break;
      }
      case State::kFinished:
      case State::kAborted:
      case State::kTimedOut:
      case State::kTransitionStateCallbackDispatched:
        break;
    }
  }
}

void ViewTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(style_tracker_);
  visitor->Trace(script_delegate_);

  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool ViewTransition::MatchForOnlyChild(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  if (!style_tracker_)
    return false;
  return style_tracker_->MatchForOnlyChild(pseudo_id, view_transition_name);
}

void ViewTransition::ContextDestroyed() {
  TRACE_EVENT0("blink", "ViewTransition::ContextDestroyed");

  // Don't try to interact with script after the Document starts shutdown.
  script_delegate_.Clear();

  // TODO(khushalsagar): This needs to be called for pages entering BFCache.
  SkipTransition(PromiseResponse::kRejectAbort);
}

void ViewTransition::NotifyCaptureFinished() {
  if (state_ != State::kCapturing) {
    DCHECK(IsTerminalState(state_));
    return;
  }
  bool process_next_state = AdvanceTo(State::kCaptured);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::NotifyDOMCallbackFinished(bool success) {
  if (IsTerminalState(state_))
    return;

  CHECK_EQ(state_, State::kDOMCallbackRunning);

  bool process_next_state = AdvanceTo(State::kDOMCallbackFinished);
  DCHECK(process_next_state);
  if (!success) {
    SkipTransition(PromiseResponse::kRejectAbort);
  }
  ProcessCurrentState();

  // Succeed or fail, rendering must be resumed after this.
  CHECK(!rendering_paused_scope_);
}

bool ViewTransition::NeedsViewTransitionEffectNode(
    const LayoutObject& object) const {
  // Layout view always needs an effect node, even if root itself is not
  // transitioning. The reason for this is that we want the root to have an
  // effect which can be hoisted up be the sibling of the layout view. This
  // simplifies calling code to have a consistent stacking context structure.
  if (IsA<LayoutView>(object))
    return !IsTerminalState(state_);

  // Otherwise check if the layout object has a transition element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && IsTransitionElementExcludingRoot(*element);
}

bool ViewTransition::NeedsViewTransitionClipNode(
    const LayoutObject& object) const {
  // The root element's painting is already clipped to the snapshot root using
  // LayoutView::ViewRect.
  if (IsA<LayoutView>(object)) {
    return false;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_ &&
         style_tracker_->NeedsCaptureClipNode(*element);
}

bool ViewTransition::IsRepresentedViaPseudoElements(
    const LayoutObject& object) const {
  if (IsTerminalState(state_)) {
    return false;
  }

  if (IsA<LayoutView>(object)) {
    return document_->documentElement() &&
           style_tracker_->IsTransitionElement(*document_->documentElement());
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && IsTransitionElementExcludingRoot(*element);
}

bool ViewTransition::IsTransitionElementExcludingRoot(
    const Element& node) const {
  if (IsTerminalState(state_)) {
    return false;
  }

  return !node.IsDocumentElement() && style_tracker_->IsTransitionElement(node);
}

PaintPropertyChangeType ViewTransition::UpdateEffect(
    const LayoutObject& object,
    const EffectPaintPropertyNodeOrAlias& current_effect,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(NeedsViewTransitionEffectNode(object));
  DCHECK(current_transform);
  DCHECK(current_clip);

  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons = CompositingReason::kViewTransitionElement;
  state.local_transform_space = current_transform;
  state.output_clip = current_clip;
  state.view_transition_element_id = ViewTransitionElementId(document_tag_);
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      object.UniqueId(), CompositorElementIdNamespace::kViewTransitionElement);
  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    // The only non-element participant is the layout view.
    DCHECK(object.IsLayoutView());

    // We always generate an effect node for the root element but element and
    // resource IDs are only setup if the root element is participating in the
    // transition, requiring a snapshot.
    if (IsRootTransitioning()) {
      style_tracker_->UpdateElementIndicesAndSnapshotId(
          document_->documentElement(), state.view_transition_element_id,
          state.view_transition_element_resource_id);
    }
    return style_tracker_->UpdateRootEffect(std::move(state), current_effect);
  }

  state.self_or_ancestor_participates_in_view_transition = true;
  style_tracker_->UpdateElementIndicesAndSnapshotId(
      element, state.view_transition_element_id,
      state.view_transition_element_resource_id);
  return style_tracker_->UpdateEffect(*element, std::move(state),
                                      current_effect);
}

PaintPropertyChangeType ViewTransition::UpdateCaptureClip(
    const LayoutObject& object,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(NeedsViewTransitionClipNode(object));
  DCHECK(current_transform);

  auto* element = DynamicTo<Element>(object.GetNode());
  DCHECK(element);
  return style_tracker_->UpdateCaptureClip(*element, current_clip,
                                           current_transform);
}

const EffectPaintPropertyNode* ViewTransition::GetEffect(
    const LayoutObject& object) const {
  DCHECK(NeedsViewTransitionEffectNode(object));

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element)
    return style_tracker_->GetRootEffect();
  return style_tracker_->GetEffect(*element);
}

const ClipPaintPropertyNode* ViewTransition::GetCaptureClip(
    const LayoutObject& object) const {
  DCHECK(NeedsViewTransitionClipNode(object));

  return style_tracker_->GetCaptureClip(*To<Element>(object.GetNode()));
}

void ViewTransition::RunViewTransitionStepsOutsideMainFrame() {
  DCHECK(document_->Lifecycle().GetState() >=
         DocumentLifecycle::kPrePaintClean);
  DCHECK(!in_main_lifecycle_update_);

  if (state_ == State::kAnimating && style_tracker_ &&
      !style_tracker_->RunPostPrePaintSteps()) {
    SkipTransition(PromiseResponse::kRejectInvalidState);
  }
}

void ViewTransition::RunViewTransitionStepsDuringMainFrame() {
  DCHECK_NE(state_, State::kWaitForRenderBlock);

  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  DCHECK(!in_main_lifecycle_update_);

  base::AutoReset<bool> scope(&in_main_lifecycle_update_, true);
  if (StateRunsInViewTransitionStepsDuringMainFrame(state_))
    ProcessCurrentState();

  if (style_tracker_ &&
      document_->Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean &&
      !style_tracker_->RunPostPrePaintSteps()) {
    SkipTransition(PromiseResponse::kRejectInvalidState);
  }
}

bool ViewTransition::NeedsUpToDateTags() const {
  return state_ == State::kCaptureTagDiscovery ||
         state_ == State::kAnimateTagDiscovery;
}

PseudoElement* ViewTransition::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) {
  DCHECK(style_tracker_);

  return style_tracker_->CreatePseudoElement(parent, pseudo_id,
                                             view_transition_name);
}

CSSStyleSheet* ViewTransition::UAStyleSheet() const {
  // TODO(vmpstr): We can still request getComputedStyle(html,
  // "::view-transition-pseudo") outside of a page transition. What should we
  // return in that case?
  if (!style_tracker_)
    return nullptr;
  return &style_tracker_->UAStyleSheet();
}

void ViewTransition::WillCommitCompositorFrame() {
  // There should only be 1 commit when we're in the capturing phase and
  // rendering is paused immediately after it finishes.
  if (state_ == State::kCapturing)
    PauseRendering();
}

gfx::Size ViewTransition::GetSnapshotRootSize() const {
  if (!style_tracker_)
    return gfx::Size();

  return style_tracker_->GetSnapshotRootSize();
}

gfx::Vector2d ViewTransition::GetFrameToSnapshotRootOffset() const {
  if (!style_tracker_)
    return gfx::Vector2d();

  return style_tracker_->GetFrameToSnapshotRootOffset();
}

void ViewTransition::PauseRendering() {
  DCHECK(!rendering_paused_scope_);

  if (!document_->GetPage() || !document_->View())
    return;

  rendering_paused_scope_.emplace(*document_);
  document_->GetPage()->GetChromeClient().UnregisterFromCommitObservation(this);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("blink", "ViewTransition::PauseRendering",
                                    this);
  const base::TimeDelta kTimeout = [this]() {
    if (auto* settings = document_->GetFrame()->GetContentSettingsClient();
        settings && settings->IncreaseViewTransitionCallbackTimeout()) {
      return base::Seconds(15);
    } else {
      return base::Seconds(4);
    }
  }();
  document_->GetTaskRunner(TaskType::kInternalFrameLifecycleControl)
      ->PostDelayedTask(FROM_HERE,
                        WTF::BindOnce(&ViewTransition::OnRenderingPausedTimeout,
                                      WrapWeakPersistent(this)),
                        kTimeout);
}

void ViewTransition::OnRenderingPausedTimeout() {
  if (!rendering_paused_scope_)
    return;

  ResumeRendering();
  SkipTransition(PromiseResponse::kRejectTimeout);
  AdvanceTo(State::kTimedOut);
}

void ViewTransition::ResumeRendering() {
  if (!rendering_paused_scope_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END0("blink", "ViewTransition::PauseRendering",
                                  this);
  rendering_paused_scope_.reset();
}

void ViewTransition::ActivateFromSnapshot() {
  if (state_ != State::kWaitForRenderBlock)
    return;

  CHECK(IsForNavigationOnNewDocument());

  // This function implies that rendering has started. If we were waiting
  // for render-blocking resources to be loaded, they must have been fetched (or
  // timed out) before rendering is started.
  DCHECK(document_->RenderingHasBegun());
  bool process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

bool ViewTransition::ShouldThrottleRendering() const {
  return rendering_paused_scope_ &&
         rendering_paused_scope_->ShouldThrottleRendering();
}

}  // namespace blink
