// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_

#include <atomic>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"

namespace cc {
class EventMetrics;
}

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

namespace blink {
namespace scheduler {
class WidgetScheduler;
}  // namespace scheduler

class CompositorThreadScheduler;
class SynchronousCompositorRegistry;
class SynchronousCompositorProxyRegistry;
class WebInputEventAttribution;
class WidgetBase;

// This class maintains the compositor InputHandlerProxy and is
// responsible for passing input events on the compositor and main threads.
// The lifecycle of this object matches that of the WidgetBase.
class PLATFORM_EXPORT WidgetInputHandlerManager final
    : public base::RefCountedThreadSafe<WidgetInputHandlerManager>,
      public InputHandlerProxyClient,
      public MainThreadEventQueueClient,
      public base::SupportsWeakPtr<WidgetInputHandlerManager> {
  // Used in UMA metrics reporting. Do not re-order, and rename the metric if
  // additional states are required.
  enum class InitialInputTiming {
    // Input comes before lifecycle update
    kBeforeLifecycle = 0,
    // Input is before commit
    kBeforeCommit = 1,
    // Input comes before first paint
    kBeforeFirstPaint = 2,
    // Input comes only after first paint
    kAfterFirstPaint = 3,
    kMaxValue = kAfterFirstPaint
  };

  // For use in bitfields to keep track of why we should keep suppressing input
  // events. Maybe the rendering pipeline is currently deferring something, or
  // we are still waiting for the user to see some non empty paint. And we use
  // the combination of states to correctly report UMA for input that is
  // suppressed.
  enum class SuppressingInputEventsBits {
    // if set, suppress events because pipeline is deferring main frame updates
    kDeferMainFrameUpdates = 1 << 0,
    // if set, suppress events because pipeline is deferring commits
    kDeferCommits = 1 << 1,
    // if set, we have not painted a main frame from the current navigation yet
    kHasNotPainted = 1 << 2,
  };

 public:
  // The `widget` and `frame_widget_input_handler` should be invalidated
  // at the same time.
  static scoped_refptr<WidgetInputHandlerManager> Create(
      base::WeakPtr<WidgetBase> widget_base,
      base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
          frame_widget_input_handler,
      bool never_composited,
      CompositorThreadScheduler* compositor_thread_scheduler,
      scoped_refptr<scheduler::WidgetScheduler> widget_scheduler,
      bool needs_input_handler,
      bool allow_scroll_resampling);

  WidgetInputHandlerManager(const WidgetInputHandlerManager&) = delete;
  WidgetInputHandlerManager& operator=(const WidgetInputHandlerManager&) =
      delete;

  void AddInterface(
      mojo::PendingReceiver<mojom::blink::WidgetInputHandler> receiver,
      mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host);

  // MainThreadEventQueueClient overrides.
  bool HandleInputEvent(const WebCoalescedInputEvent& event,
                        std::unique_ptr<cc::EventMetrics> metrics,
                        HandledEventCallback handled_callback) override;
  void InputEventsDispatched(bool raf_aligned) override;
  void SetNeedsMainFrame() override;

  void DidFirstVisuallyNonEmptyPaint(const base::TimeTicks& first_paint_time);

  // InputHandlerProxyClient overrides.
  void WillShutdown() override;
  void DispatchNonBlockingEventToMainThread(
      std::unique_ptr<WebCoalescedInputEvent> event,
      const WebInputEventAttribution& attribution,
      std::unique_ptr<cc::EventMetrics> metrics) override;

  void DidAnimateForInput() override;
  void DidStartScrollingViewport() override;
  void GenerateScrollBeginAndSendToMainThread(
      const WebGestureEvent& update_event,
      const WebInputEventAttribution& attribution,
      const cc::EventMetrics* update_metrics) override;
  void SetAllowedTouchAction(cc::TouchAction touch_action) override;
  bool AllowsScrollResampling() override { return allow_scroll_resampling_; }

  void ObserveGestureEventOnMainThread(
      const WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void DispatchScrollGestureToCompositor(
      std::unique_ptr<WebGestureEvent> event);
  void DispatchEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback);

  void ProcessTouchAction(cc::TouchAction touch_action);

  mojom::blink::WidgetInputHandlerHost* GetWidgetInputHandlerHost();

#if BUILDFLAG(IS_ANDROID)
  void AttachSynchronousCompositor(
      mojo::PendingRemote<mojom::blink::SynchronousCompositorControlHost>
          control_host,
      mojo::PendingAssociatedRemote<mojom::blink::SynchronousCompositorHost>
          host,
      mojo::PendingAssociatedReceiver<mojom::blink::SynchronousCompositor>
          compositor_request);

  SynchronousCompositorRegistry* GetSynchronousCompositorRegistry();
#endif

  void InvokeInputProcessedCallback();
  void InputWasProcessed(const gfx::PresentationFeedback& feedback);
  void WaitForInputProcessed(base::OnceClosure callback);

  // Called when the WidgetBase is notified of a navigation. Resets
  // the suppressing of input events state, and resets the UMA recorder for
  // time of first input.
  void DidNavigate();

  // Called to inform us when the system starts or stops main frame updates.
  void OnDeferMainFrameUpdatesChanged(bool);

  // Called to inform us when the system starts or stops deferring commits.
  void OnDeferCommitsChanged(bool defer_status, cc::PaintHoldingReason reason);

  // Allow tests, headless etc. to have input events processed before the
  // compositor is ready to commit frames.
  // TODO(schenney): Fix this somehow, forcing all tests to wait for
  // hit test regions.
  void AllowPreCommitInput() { allow_pre_commit_input_ = true; }

  // Called on the main thread. Finds the matching element under the given
  // point in visual viewport coordinates and runs the callback with the
  // found element id on input thread task runner.
  using ElementAtPointCallback = base::OnceCallback<void(cc::ElementId)>;
  void FindScrollTargetOnMainThread(const gfx::PointF& point,
                                    ElementAtPointCallback callback);
  void SendDroppedPointerDownCounts();

  void ClearClient();

  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate);

  MainThreadEventQueue* input_event_queue() { return input_event_queue_.get(); }

  base::SingleThreadTaskRunner* main_task_runner_for_testing() const {
    return main_thread_task_runner_.get();
  }

 protected:
  friend class base::RefCountedThreadSafe<WidgetInputHandlerManager>;
  ~WidgetInputHandlerManager() override;

 private:
  WidgetInputHandlerManager(
      base::WeakPtr<WidgetBase> widget,
      base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
          frame_widget_input_handler,
      bool never_composited,
      CompositorThreadScheduler* compositor_thread_scheduler,
      scoped_refptr<scheduler::WidgetScheduler> widget_scheduler,
      bool allow_scroll_resampling);
  void InitInputHandler();
  void InitOnInputHandlingThread(
      const base::WeakPtr<cc::CompositorDelegateForInput>& compositor_delegate,
      bool sync_compositing);
  void BindChannel(
      mojo::PendingReceiver<mojom::blink::WidgetInputHandler> receiver);

  // This method skips the input handler proxy and sends the event directly to
  // the WidgetBase (main thread). Should only be used by non-frame
  // WidgetBase that don't use a compositor (e.g. popups, plugins). Events
  // for a frame WidgetBase should always be passed through the
  // InputHandlerProxy by calling DispatchEvent which will re-route to the main
  // thread if needed.
  void DispatchDirectlyToWidget(
      std::unique_ptr<WebCoalescedInputEvent> event,
      std::unique_ptr<cc::EventMetrics> metrics,
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback);

  // Used to return a result from FindScrollTargetOnMainThread. Will be called
  // on the input handling thread.
  void FindScrollTargetReply(
      std::unique_ptr<WebCoalescedInputEvent> event,
      std::unique_ptr<cc::EventMetrics> metrics,
      mojom::blink::WidgetInputHandler::DispatchEventCallback browser_callback,
      cc::ElementId hit_test_result);

  // This method is the callback used by the compositor input handler to
  // communicate back whether the event was successfully handled on the
  // compositor thread or whether it needs to forwarded to the main thread.
  // This method is responsible for passing the `event` and its accompanying
  // `metrics` on to the main thread or replying to the browser that the event
  // was handled. This is always called on the input handling thread (i.e. if a
  // compositor thread exists, it'll be called from it).
  void DidHandleInputEventSentToCompositor(
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
      InputHandlerProxy::EventDisposition event_disposition,
      std::unique_ptr<WebCoalescedInputEvent> event,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll_params,
      const WebInputEventAttribution& attribution,
      std::unique_ptr<cc::EventMetrics> metrics,
      mojom::blink::ScrollResultDataPtr scroll_result_data);

  // Similar to the above; this is used by the main thread input handler to
  // communicate back the result of handling the event. Note: this may be
  // called on either thread as non-blocking events sent to the main thread
  // will be ACKed immediately when added to the main thread event queue.
  void DidHandleInputEventSentToMain(
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
      absl::optional<cc::TouchAction> touch_action_from_compositor,
      mojom::blink::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      mojom::blink::DidOverscrollParamsPtr overscroll_params,
      absl::optional<cc::TouchAction> touch_action_from_main);

  // This method calls into DidHandleInputEventSentToMain but has a
  // slightly different signature. TODO(dtapuska): Remove this
  // method once InputHandlerProxy is no longer on the blink public API
  // and can use mojom blink types directly.
  void DidHandleInputEventSentToMainFromWidgetBase(
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
      mojom::blink::InputEventResultState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<blink::InputHandlerProxy::DidOverscrollParams>
          overscroll_params,
      absl::optional<cc::TouchAction> touch_action);

  void ObserveGestureEventOnInputHandlingThread(
      const WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void HandleInputEventWithLatencyOnInputHandlingThread(
      std::unique_ptr<WebCoalescedInputEvent>);

  // The kInputBlocking task runner is for tasks which are on the critical path
  // of showing the effect of an already-received input event, and should be
  // prioritized above handling new input.
  enum class TaskRunnerType { kDefault = 0, kInputBlocking = 1 };

  // Returns the task runner for the thread that receives input. i.e. the
  // "Mojo-bound" thread.
  const scoped_refptr<base::SingleThreadTaskRunner>& InputThreadTaskRunner(
      TaskRunnerType type = TaskRunnerType::kDefault) const;

  void LogInputTimingUMA();

  void RecordMetricsForDroppedEventsBeforePaint(const base::TimeTicks&);

  // Only valid to be called on the main thread.
  base::WeakPtr<WidgetBase> widget_;
  base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
      frame_widget_input_handler_;
  scoped_refptr<scheduler::WidgetScheduler> widget_scheduler_;

  // InputHandlerProxy is only interacted with on the compositor
  // thread.
  std::unique_ptr<InputHandlerProxy> input_handler_proxy_;

  // The WidgetInputHandlerHost is bound on the compositor task runner
  // but class can be called on the compositor and main thread.
  mojo::SharedRemote<mojom::blink::WidgetInputHandlerHost> host_;

  // Any thread can access these variables.
  scoped_refptr<MainThreadEventQueue> input_event_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      compositor_thread_default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      compositor_thread_input_blocking_task_runner_;

  // The touch action that InputHandlerProxy has asked us to allow. This should
  // only be accessed on the compositor thread!
  absl::optional<cc::TouchAction> compositor_allowed_touch_action_;

  // Callback used to respond to the WaitForInputProcessed Mojo message. This
  // callback is set from and must be invoked from the Mojo-bound thread (i.e.
  // the InputThreadTaskRunner thread), it will invoke the Mojo reply.
  base::OnceClosure input_processed_callback_;

  // Whether this widget uses an InputHandler or forwards all input to the
  // WebWidget (Popups, Plugins). This is always true if we have a compositor
  // thread; however, we can use an input handler if we don't have a compositor
  // thread (e.g. in tests). Conversely, if we're not using an input handler,
  // we definitely don't have a compositor thread.
  bool uses_input_handler_ = false;

  struct UmaData {
    // Saves the number of events that would be dropped by the
    // DropInputEventsBeforeFirstPaint feature (i.e. before receiving the first
    // presentation of content). This is important because it shows how many
    // times user tried to interact with page but the event was dropped.
    int suppressed_events_count_ = 0;

    // Saves most recent input event time that would be dropped by the
    // DropInputEventsBeforeFirstPaint feature (i.e. before receiving the first
    // presentation of content). If this is after the first paint timestamp,
    // we log the difference to track the worst dropped event experienced.
    base::TimeTicks most_recent_suppressed_event_time_;

    // Control of UMA. We emit one UMA metric per navigation telling us
    // whether any non-move input arrived before we starting updating the page
    // or displaying content to the user. It must be atomic because navigation
    // can occur on the renderer thread (resetting this) coincident with the UMA
    // being sent on the compositor thread.
    bool have_emitted_uma_{false};
  };

  base::Lock uma_data_lock_;
  UmaData uma_data_;

  // State tracking why we should keep suppressing input events, keeps track of
  // which parts of the rendering pipeline are currently deferred, or whether
  // we are waiting for the first non empty paint. We use this state to suppress
  // all events while user has not seen first paint or rendering pipeline is
  // deferring something.
  // Move events are still processed to allow tracking of mouse position.
  // Metrics also report the lifecycle state when the first non-move event is
  // seen.
  // This is a bitfield, using the bit values from SuppressingInputEventsBits.
  // The compositor thread accesses this value when processing input (to decide
  // whether to suppress input) and the renderer thread accesses it when the
  // status of deferrals changes, so it needs to be thread safe.
  std::atomic<uint16_t> suppressing_input_events_state_;

  // Allow input suppression to be disabled for tests and non-browser uses
  // of chromium that do not wait for the first commit, or that may never
  // commit. Over time, tests should be fixed so they provide additional
  // coverage for input suppression: crbug.com/987626
  bool allow_pre_commit_input_ = false;

  // Specifies weather the renderer has received a scroll-update event after the
  // last scroll-begin or not, It is used to determine whether a scroll-update
  // is the first one in a scroll sequence or not. This variable is only used on
  // the input handling thread (i.e. on the compositor thread if it exists).
  bool has_seen_first_gesture_scroll_update_after_begin_ = false;

  std::unique_ptr<power_scheduler::PowerModeVoter> response_power_mode_voter_;

  // Timer for count dropped events.
  std::unique_ptr<base::OneShotTimer> dropped_event_counts_timer_;

  unsigned dropped_pointer_down_ = 0;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SynchronousCompositorProxyRegistry>
      synchronous_compositor_registry_;
#endif

  // Whether to use ScrollPredictor to resample scroll events. This is false for
  // web_tests to ensure that scroll deltas are not timing-dependent.
  const bool allow_scroll_resampling_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
