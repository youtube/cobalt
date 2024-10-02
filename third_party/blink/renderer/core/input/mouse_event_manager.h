// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/input/pointer_id.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/input/boundary_event_dispatcher.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink-forward.h"

namespace blink {

class ContainerNode;
class DragState;
class DataTransfer;
class Element;
class HitTestResult;
class InputDeviceCapabilities;
class LocalFrame;
class ScrollManager;

enum class DragInitiator;

// This class takes care of dispatching all mouse events and keeps track of
// positions and states of mouse.
class CORE_EXPORT MouseEventManager final
    : public GarbageCollected<MouseEventManager>,
      public SynchronousMutationObserver {
 public:
  MouseEventManager(LocalFrame&, ScrollManager&);
  MouseEventManager(const MouseEventManager&) = delete;
  MouseEventManager& operator=(const MouseEventManager&) = delete;
  virtual ~MouseEventManager();
  void Trace(Visitor*) const override;

  WebInputEventResult DispatchMouseEvent(
      EventTarget*,
      const AtomicString&,
      const WebMouseEvent&,
      const gfx::PointF* last_position,
      EventTarget* related_target,
      bool check_for_listener = false,
      const PointerId& pointer_id = PointerEventFactory::kInvalidId,
      const String& pointer_type = g_empty_string);

  WebInputEventResult SetMousePositionAndDispatchMouseEvent(
      Element* target_element,
      const AtomicString& event_type,
      const WebMouseEvent&);

  WebInputEventResult DispatchMouseClickIfNeeded(
      Element* mouse_release_target,
      Element* captured_click_target,
      const WebMouseEvent& mouse_event,
      const PointerId& pointer_id,
      const String& pointer_type);

  WebInputEventResult DispatchDragSrcEvent(const AtomicString& event_type,
                                           const WebMouseEvent&);
  WebInputEventResult DispatchDragEvent(const AtomicString& event_type,
                                        Node* target,
                                        Node* related_target,
                                        const WebMouseEvent&,
                                        DataTransfer*);

  // Resets the internal state of this object.
  void Clear();

  void SendBoundaryEvents(EventTarget* exited_target,
                          EventTarget* entered_target,
                          const WebMouseEvent&);

  void SetElementUnderMouse(Element*,
                            const WebMouseEvent&);

  WebInputEventResult HandleMouseFocus(
      const HitTestResult&,
      InputDeviceCapabilities* source_capabilities);

  void SetLastKnownMousePosition(const WebMouseEvent&);
  void SetLastMousePositionAsUnknown();

  bool HandleDragDropIfPossible(const GestureEventWithHitTestResults&);

  WebInputEventResult HandleMouseDraggedEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult HandleMousePressEvent(
      const MouseEventWithHitTestResults&);
  WebInputEventResult HandleMouseReleaseEvent(
      const MouseEventWithHitTestResults&);

  DragState& GetDragState();

  void FocusDocumentView();

  // Resets the state that indicates the next events could cause a drag. It is
  // called when we realize the next events should not cause drag based on the
  // drag heuristics.
  void ClearDragHeuristicState();

  void DragSourceEndedAt(const WebMouseEvent&, ui::mojom::blink::DragOperation);

  void UpdateSelectionForMouseDrag();

  void HandleMousePressEventUpdateStates(const WebMouseEvent&);
  void HandleMouseReleaseEventUpdateStates();

  // Returns whether pan is handled and resets the state on release.
  bool HandleSvgPanIfNeeded(bool is_release_event);

  void InvalidateClick();

  // TODO: These functions ideally should be private but the code needs more
  // refactoring to be able to remove the dependency from EventHandler.
  Element* GetElementUnderMouse();
  bool IsMousePositionUnknown();
  gfx::PointF LastKnownMousePositionInViewport();
  gfx::PointF LastKnownMouseScreenPosition();

  bool MousePressed();
  void ReleaseMousePress();

  bool CapturesDragging() const;
  void SetCapturesDragging(bool);

  void SetMouseDownMayStartAutoscroll() {
    mouse_down_may_start_autoscroll_ = true;
  }

  Node* MousePressNode();
  void SetMousePressNode(Node*);

  Element* ClickElement();

  void SetClickElement(Element*);
  void SetClickCount(int);

  bool MouseDownMayStartDrag();

  void RecomputeMouseHoverStateIfNeeded();
  void RecomputeMouseHoverState();

  void MarkHoverStateDirty();

 private:
  class MouseEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
   public:
    MouseEventBoundaryEventDispatcher(MouseEventManager*, const WebMouseEvent*);
    MouseEventBoundaryEventDispatcher(
        const MouseEventBoundaryEventDispatcher&) = delete;
    MouseEventBoundaryEventDispatcher& operator=(
        const MouseEventBoundaryEventDispatcher&) = delete;

   protected:
    void DispatchOut(EventTarget*, EventTarget* related_target) override;
    void DispatchOver(EventTarget*, EventTarget* related_target) override;
    void DispatchLeave(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    void DispatchEnter(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    AtomicString GetLeaveEvent() override;
    AtomicString GetEnterEvent() override;

   private:
    void Dispatch(EventTarget*,
                  EventTarget* related_target,
                  const AtomicString&,
                  const WebMouseEvent&,
                  bool check_for_listener);
    MouseEventManager* mouse_event_manager_;
    const WebMouseEvent* web_mouse_event_;
  };

  // If the given element is a shadow host and its root has delegatesFocus=false
  // flag, slide focus to its inner element. Returns true if the resulting focus
  // is different from the given element.
  bool SlideFocusOnShadowHostIfNecessary(const Element&);

  bool DragThresholdExceeded(const gfx::Point&) const;
  bool HandleDrag(const MouseEventWithHitTestResults&, DragInitiator);
  bool TryStartDrag(const MouseEventWithHitTestResults&);
  void ClearDragDataTransfer();
  DataTransfer* CreateDraggingDataTransfer() const;

  void ResetDragSource();
  bool HoverStateDirty();

  // Implementations of |SynchronousMutationObserver|
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node& node_to_be_removed) final;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |MouseEventManager::clear()|.

  const Member<LocalFrame> frame_;
  Member<ScrollManager> scroll_manager_;

  // The effective position of the mouse pointer.
  // See
  // https://w3c.github.io/pointerevents/#dfn-tracking-the-effective-position-of-the-legacy-mouse-pointer.
  Member<Element> element_under_mouse_;

  // The last mouse movement position this frame has seen in viewport
  // coordinates.
  PhysicalOffset last_known_mouse_position_in_root_frame_;
  gfx::PointF last_known_mouse_position_;
  gfx::PointF last_known_mouse_screen_position_;

  unsigned is_mouse_position_unknown_ : 1;
  // Current button-press state for mouse/mouse-like-stylus.
  // TODO(crbug.com/563676): Buggy for chorded buttons.
  unsigned mouse_pressed_ : 1;

  unsigned mouse_down_may_start_autoscroll_ : 1;
  unsigned svg_pan_ : 1;
  unsigned captures_dragging_ : 1;
  unsigned mouse_down_may_start_drag_ : 1;

  Member<Node> mouse_press_node_;

  int click_count_;
  Member<Element> click_element_;

  gfx::Point mouse_down_pos_;
  base::TimeTicks mouse_down_timestamp_;
  WebMouseEvent mouse_down_;

  PhysicalOffset drag_start_pos_in_root_frame_;
  // This indicates that whether we should update the hover at each begin
  // frame. This is set to be true after the compositor or main thread scroll
  // ends, and at each begin frame, we will dispatch a fake mouse move event to
  // update hover when this is true.
  bool hover_state_dirty_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_MOUSE_EVENT_MANAGER_H_
