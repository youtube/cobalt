// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_

#include <memory>

#include "base/containers/span.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_light_probe_init.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class Element;
class ExceptionState;
class HTMLCanvasElement;
class ResizeObserver;
class ScriptPromiseResolver;
class V8XRFrameRequestCallback;
class XRAnchor;
class XRAnchorSet;
class XRCanvasInputProvider;
class XRCPUDepthInformation;
class XRDepthManager;
class XRDOMOverlayState;
class XRHitTestOptionsInit;
class XRHitTestSource;
class XRImageTrackingResult;
class XRLightProbe;
class XRPlaneSet;
class XRPlaneManager;
class XRReferenceSpace;
class XRRenderState;
class XRRenderStateInit;
class XRSessionViewportScaler;
class XRSpace;
class XRSystem;
class XRTransientInputHitTestOptionsInit;
class XRTransientInputHitTestSource;
class XRViewData;
class XRWebGLDepthInformation;
class XRWebGLLayer;

using XRSessionFeatureSet = HashSet<device::mojom::XRSessionFeature>;

class XRSession final
    : public EventTargetWithInlineData,
      public device::mojom::blink::XRSessionClient,
      public device::mojom::blink::XRInputSourceButtonListener,
      public ActiveScriptWrappable<XRSession> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Error strings used outside of XRSession:
  static constexpr char kNoRigidTransformSpecified[] =
      "No XRRigidTransform specified.";
  static constexpr char kUnableToRetrieveMatrix[] =
      "The operation was unable to retrieve a matrix from passed in space and "
      "could not be completed.";
  static constexpr char kNoSpaceSpecified[] = "No XRSpace specified.";
  static constexpr char kAnchorsFeatureNotSupported[] =
      "Anchors feature is not supported by the session.";
  static constexpr char kPlanesFeatureNotSupported[] =
      "Plane detection feature is not supported by the session.";
  static constexpr char kDepthSensingFeatureNotSupported[] =
      "Depth sensing feature is not supported by the session.";
  static constexpr char kRawCameraAccessFeatureNotSupported[] =
      "Raw camera access feature is not supported by the session.";
  static constexpr char kCannotCancelHitTestSource[] =
      "Hit test source could not be canceled! Ensure that it was not already "
      "canceled.";
  static constexpr char kCannotReportPoses[] =
      "Poses cannot be given out for the current state.";

  // Runs all the video.requestVideoFrameCallback() callbacks associated with
  // one HTMLVideoElement. |double| is the |high_res_now_ms|, derived from
  // MonotonicTimeToZeroBasedDocumentTime(|current_frame_time|), to be passed as
  // the "now" parameter when executing rVFC callbacks. In other words, a
  // video.rVFC and an xrSession.rAF callback share the same "now" parameters if
  // they are run in the same turn of the render loop.
  using ExecuteVfcCallback = base::OnceCallback<void(double)>;

  struct MetricsReporter {
    explicit MetricsReporter(
        mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder);

    // Reports a use (or attempted use) of the given feature to the underlying
    // metrics recorder.
    void ReportFeatureUsed(device::mojom::blink::XRSessionFeature feature);

   private:
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder_;

    // Keeps track of which features have already been reported, to reduce
    // redundant mojom calls.
    HashSet<device::mojom::blink::XRSessionFeature> reported_features_;
  };

  XRSession(XRSystem* xr,
            mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
                client_receiver,
            device::mojom::blink::XRSessionMode mode,
            device::mojom::blink::XREnvironmentBlendMode environment_blend_mode,
            device::mojom::blink::XRInteractionMode interaction_mode,
            device::mojom::blink::XRSessionDeviceConfigPtr device_config,
            bool sensorless_session,
            XRSessionFeatureSet enabled_features);
  ~XRSession() override = default;

  XRSystem* xr() const { return xr_; }
  const String& environmentBlendMode() const { return blend_mode_string_; }
  const String& interactionMode() const { return interaction_mode_string_; }
  XRDOMOverlayState* domOverlayState() const { return dom_overlay_state_; }
  const String visibilityState() const;
  absl::optional<float> frameRate() const { return absl::nullopt; }
  DOMFloat32Array* supportedFrameRates() const { return nullptr; }
  XRRenderState* renderState() const { return render_state_; }

  // ARCore by default returns textures in RGBA half-float HDR format and no
  // other runtimes support reflection mapping, so just return this until we
  // have a need to differentiate based on the underlying runtime.
  const String preferredReflectionFormat() const { return "rgba16f"; }

  Vector<String> enabledFeatures() const;

  XRSpace* viewerSpace() const;

  XRAnchorSet* TrackedAnchors() const;

  bool immersive() const;
  device::mojom::blink::XRSessionMode mode() const { return mode_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(select, kSelect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inputsourceschange, kInputsourceschange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart, kSelectstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectend, kSelectend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(visibilitychange, kVisibilitychange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(squeeze, kSqueeze)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(squeezestart, kSqueezestart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(squeezeend, kSqueezeend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(frameratechange, kFrameratechange)

  void updateRenderState(XRRenderStateInit* render_state_init,
                         ExceptionState& exception_state);

  const String& depthUsage(ExceptionState& exception_state);
  const String& depthDataFormat(ExceptionState& exception_state);

  ScriptPromise updateTargetFrameRate(float rate,
                                      ExceptionState&);

  ScriptPromise requestReferenceSpace(ScriptState* script_state,
                                      const String& type,
                                      ExceptionState&);

  // Helper, not IDL-exposed
  // |native_origin_from_anchor| is a matrix describing transform between native
  // origin and the initial anchor's position.
  // |native_origin_information| describes native origin relative to which the
  // transform is expressed.
  // |maybe_plane_id| is an ID of the plane to which the anchor should be
  // attached - set to absl::nullopt if the plane is not to be attached to any
  // plane.
  ScriptPromise CreateAnchorHelper(
      ScriptState* script_state,
      const gfx::Transform& native_origin_from_anchor,
      const device::mojom::blink::XRNativeOriginInformationPtr&
          native_origin_information,
      absl::optional<uint64_t> maybe_plane_id,
      ExceptionState& exception_state);

  // Helper POD type containing the information needed for anchor creation in
  // case the anchor needs to be transformed to be expressed relative to a
  // stationary reference space.
  struct ReferenceSpaceInformation {
    device::mojom::blink::XRNativeOriginInformationPtr native_origin;
    gfx::Transform mojo_from_space;
  };

  // Helper for anchor creation - returns information about the reference space
  // type and its transform. The resulting reference space will be well-suited
  // for anchor creation (i.e. the native origin set in the struct will be
  // describing a stationary space). If a stationary reference space is not
  // available, the method returns nullopt.
  absl::optional<ReferenceSpaceInformation> GetStationaryReferenceSpace() const;

  int requestAnimationFrame(V8XRFrameRequestCallback* callback);
  void cancelAnimationFrame(int id);

  XRInputSourceArray* inputSources(ScriptState*) const;

  ScriptPromise requestHitTestSource(ScriptState* script_state,
                                     XRHitTestOptionsInit* options,
                                     ExceptionState& exception_state);
  ScriptPromise requestHitTestSourceForTransientInput(
      ScriptState* script_state,
      XRTransientInputHitTestOptionsInit* options_init,
      ExceptionState& exception_state);

  ScriptPromise requestLightProbe(ScriptState* script_state,
                                  XRLightProbeInit*,
                                  ExceptionState&);

  ScriptPromise getTrackedImageScores(ScriptState* script_state,
                                      ExceptionState&);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState* script_state, ExceptionState&);

  bool ended() const { return ended_; }

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  enum class ShutdownPolicy {
    kWaitForResponse,  // expect a future OnExitPresent call
    kImmediate,        // do all cleanup immediately
  };
  void ForceEnd(ShutdownPolicy);

  // Describes the scalar to be applied to the default framebuffer dimensions
  // which gives 1:1 pixel ratio at the center of the user's view.
  double NativeFramebufferScale() const;

  double RecommendedFramebufferScale() const;

  // Describes the recommended dimensions of layer framebuffers. Should be a
  // value that provides a good balance between quality and performance.
  gfx::SizeF RecommendedFramebufferSize() const;

  // Reports the size of the output canvas, if one is available. If not
  // reports (0, 0);
  gfx::Size OutputCanvasSize() const;
  void DetachOutputCanvas(HTMLCanvasElement* output_canvas);

  void SetDOMOverlayElement(Element* element);

  void LogGetPose() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocusChanged();
  void OnFrame(
      double timestamp,
      const absl::optional<gpu::MailboxHolder>& output_mailbox_holder,
      const absl::optional<gpu::MailboxHolder>& camera_image_mailbox_holder);

  // XRInputSourceButtonListener
  void OnButtonEvent(
      device::mojom::blink::XRInputSourceStatePtr input_source) override;

  const HeapVector<Member<XRViewData>>& views();

  void AddTransientInputSource(XRInputSource* input_source);
  void RemoveTransientInputSource(XRInputSource* input_source);

  void OnMojoSpaceReset();

  const device::mojom::blink::VRStageParametersPtr& GetStageParameters() const {
    return stage_parameters_;
  }

  mojo::PendingAssociatedRemote<
      device::mojom::blink::XRInputSourceButtonListener>
  GetInputClickListener();

  bool EmulatedPosition() const {
    // If we don't have display info then we should be using the identity
    // reference space, which by definition will be emulating the position.
    if (pending_views_.empty()) {
      return true;
    }

    return emulated_position_;
  }

  // Immersive sessions currently use two views for VR, and only a single view
  // for smartphone immersive AR mode.
  bool StereoscopicViews() { return pending_views_.size() >= 2; }

  void UpdateViews(const Vector<device::mojom::blink::XRViewPtr>& views);
  void UpdateStageParameters(
      uint32_t stage_parameters_id,
      const device::mojom::blink::VRStageParametersPtr& stage_parameters);
  // Incremented every time stage_parameters_ is changed, so that other objects
  // that depend on it can know when they need to update.
  uint32_t StageParametersId() const { return stage_parameters_id_; }

  // Returns true if the session recognizes passed in hit_test_source as still
  // existing. Intended to be used by XRFrame to implement
  // XRFrame.getHitTestResults() &
  // XRFrame.getHitTestResultsForTransientInput().
  bool ValidateHitTestSourceExists(XRHitTestSource* hit_test_source) const;
  bool ValidateHitTestSourceExists(
      XRTransientInputHitTestSource* hit_test_source) const;

  // Removes hit test source (effectively unsubscribing from the hit test).
  // Intended to be used by hit test source interfaces (XRHitTestSource and
  // XRTransientInputHitTestSource) to implement cancel() method. Returns true
  // if hit test source existed and was removed, false otherwise.
  bool RemoveHitTestSource(XRHitTestSource* hit_test_source);
  bool RemoveHitTestSource(XRTransientInputHitTestSource* hit_test_source);

  bool UsesInputEventing() { return uses_input_eventing_; }
  bool LightEstimationEnabled() { return !!world_light_probe_; }

  void Trace(Visitor* visitor) const override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  bool CanReportPoses() const;

  // Return whether we should enable anti-aliasing for WebGL layers. Value
  // comes from the underlying XR runtime.
  bool CanEnableAntiAliasing() const;

  // Returns current transform from mojo space to the space of the passed in
  // type. May return nullopt if poses cannot be reported or if the transform is
  // unknown.
  // Note: currently, the information about the mojo_from_-floor-type spaces is
  // stored elsewhere, this method will not work for those reference space
  // types.
  absl::optional<gfx::Transform> GetMojoFrom(
      device::mojom::blink::XRReferenceSpaceType space_type) const;

  XRCPUDepthInformation* GetCpuDepthInformation(
      const XRFrame* xr_frame,
      ExceptionState& exception_state) const;

  XRWebGLDepthInformation* GetWebGLDepthInformation(
      const XRFrame* xr_frame,
      ExceptionState& exception_state) const;

  XRPlaneSet* GetDetectedPlanes() const;

  // Creates presentation frame based on current state of the session.
  // The created XRFrame will store a reference to this XRSession and use it to
  // get the latest information out of it.
  XRFrame* CreatePresentationFrame(bool is_animation_frame = false);

  // Updates the internal XRSession state that is relevant to creating
  // presentation frames.
  void UpdatePresentationFrameState(
      double timestamp,
      const device::mojom::blink::VRPosePtr& mojo_from_viewer_pose,
      const device::mojom::blink::XRFrameDataPtr& frame_data,
      int16_t frame_id,
      bool emulated_position);

  // Notifies immersive session that the environment integration provider has
  // been created by the session's XR instance, |xr_|. Gives a session an
  // opportunity to register its own error handlers on environment integration
  // provider endpoint.
  void OnEnvironmentProviderCreated();

  // Returns whether the given feature is enabled for this session.
  bool IsFeatureEnabled(device::mojom::XRSessionFeature feature) const;

  // Sets the metrics reporter for this session. This should only be done once.
  void SetMetricsReporter(std::unique_ptr<MetricsReporter> reporter);

  // Queues up the execution of video.requestVideoFrameCallback() callbacks for
  // a specific HTMLVideoELement, for the next requestAnimationFrame() call.
  void ScheduleVideoFrameCallbacksExecution(ExecuteVfcCallback);

  HeapVector<Member<XRImageTrackingResult>> ImageTrackingResults(
      ExceptionState&);

  const absl::optional<gfx::Size>& CameraImageSize() const {
    return camera_image_size_;
  }

 private:
  class XRSessionResizeObserverDelegate;

  using XRVisibilityState = device::mojom::blink::XRVisibilityState;

  void UpdateCanvasDimensions(Element*);
  void ApplyPendingRenderState();

  void MaybeRequestFrame();

  void OnInputStateChangeInternal(
      int16_t frame_id,
      base::span<const device::mojom::blink::XRInputSourceStatePtr>
          input_states);
  void ProcessInputSourceEvents(
      base::span<const device::mojom::blink::XRInputSourceStatePtr>
          input_states);

  // Processes world understanding state for current frame:
  // - updates state of hit test sources & fills them out with results
  // - updates state of detected planes
  // - updates state of anchors
  // In order to correctly set the state of hit test sources, this *must* be
  // called after updating XRInputSourceArray (performed by
  // OnInputStateChangeInternal) as hit test results for transient input sources
  // use the mapping of input source id to XRInputSource object.
  void UpdateWorldUnderstandingStateForFrame(
      double timestamp,
      const device::mojom::blink::XRFrameDataPtr& frame_data);

  // XRSessionClient
  void OnExitPresent() override;
  void OnVisibilityStateChanged(
      device::mojom::blink::XRVisibilityState visibility_state) override;

  void UpdateVisibilityState();

  void OnSubscribeToHitTestResult(
      ScriptPromiseResolver* resolver,
      device::mojom::SubscribeToHitTestResult result,
      uint64_t subscription_id);

  void OnSubscribeToHitTestForTransientInputResult(
      ScriptPromiseResolver* resolver,
      device::mojom::SubscribeToHitTestResult result,
      uint64_t subscription_id);

  void OnCreateAnchorResult(ScriptPromiseResolver* resolver,
                            device::mojom::CreateAnchorResult result,
                            uint64_t id);

  void EnsureEnvironmentErrorHandler();
  void OnEnvironmentProviderError();

  void ProcessAnchorsData(
      const device::mojom::blink::XRAnchorsData* tracked_anchors_data,
      double timestamp);

  void CleanUpUnusedHitTestSources();

  void ProcessHitTestData(
      const device::mojom::blink::XRHitTestSubscriptionResultsData*
          hit_test_data);

  void ProcessTrackedImagesData(
      const device::mojom::blink::XRTrackedImagesData*);
  HeapVector<Member<XRImageTrackingResult>> frame_tracked_images_;
  bool tracked_image_scores_available_ = false;
  Vector<String> tracked_image_scores_;
  HeapVector<Member<ScriptPromiseResolver>> image_scores_resolvers_;

  void HandleShutdown();

  void ExecuteVideoFrameCallbacks(double timestamp);

  // Helper, creates an instance of depth manager if depth sensing API is
  // enabled in the session configuration.
  XRDepthManager* CreateDepthManagerIfEnabled(
      const XRSessionFeatureSet& feature_set,
      const device::mojom::blink::XRSessionDeviceConfig& device_config);

  const Member<XRSystem> xr_;
  const device::mojom::blink::XRSessionMode mode_;
  const bool environment_integration_;
  String blend_mode_string_;
  String interaction_mode_string_;
  XRVisibilityState device_visibility_state_ = XRVisibilityState::VISIBLE;
  XRVisibilityState visibility_state_ = XRVisibilityState::VISIBLE;
  String visibility_state_string_;
  Member<XRRenderState> render_state_;

  Member<XRLightProbe> world_light_probe_;
  HeapVector<Member<XRRenderStateInit>> pending_render_state_;

  // Handle delayed events and promises for session shutdown. A JS-initiated
  // end() call call marks the session as ended, but doesn't resolve the end
  // promise or trigger the 'end' event until the device side reports
  // OnExitPresent is complete. If the session end was initiated from the device
  // side, or in case of connection errors, proceed to shutdown_complete_ state
  // immediately.
  Member<ScriptPromiseResolver> end_session_resolver_;
  // "ended_" becomes true as soon as session shutdown is initiated.
  bool ended_ = false;
  bool waiting_for_shutdown_ = false;

  XRSessionFeatureSet enabled_features_;
  std::unique_ptr<MetricsReporter> metrics_reporter_;

  // From device's perspective, anchor creation is a multi-step process:
  // - phase 1: application asked to create the anchor, blink has reached out to
  // the device & does not know anything about the anchor yet.
  // - phase 2: device returned anchor ID to blink, but blink cannot
  // "materialize" the XRAnchor object & resolve the promise yet since it needs
  // to wait for more information about the anchor that will arrive through the
  // callback to XRFrameDataProvider's GetFrameData call.
  // - phase 3: device returned anchor information through XRFrameData, blink
  // can now construct XRAnchor object and resolve app's promise.
  //
  // This is done to ensure that from application's perspective, the anchor
  // creation promises get resolved only when the XRAnchor object is already
  // fully constructed.
  //
  // Anchor promises that are in phase 1 of creation live in
  // |create_anchor_promises_|, anchor promises in phase 2 live in
  // |anchor_ids_to_pending_anchor_promises_|, and anchors that got created in
  // phase 3 live in |anchor_ids_to_anchors_|.

  HeapHashMap<uint64_t, Member<XRAnchor>> anchor_ids_to_anchors_;

  // Set of promises returned from CreateAnchor that are still in-flight to the
  // device. Once the device calls us back with the newly created anchor id, the
  // resolver will be moved to |anchor_ids_to_pending_anchor_promises_|.
  HeapHashSet<Member<ScriptPromiseResolver>> create_anchor_promises_;
  // Promises for which anchors have already been created on the device side but
  // have not yet been resolved as their data is not yet available to blink.
  // Next frame update should contain the necessary data - the promise will be
  // resolved then.
  HeapHashMap<uint64_t, Member<ScriptPromiseResolver>>
      anchor_ids_to_pending_anchor_promises_;

  // Mapping of hit test source ids (aka hit test subscription ids) to hit test
  // sources. Hit test source has to be stored via weak member - JavaScript side
  // can communicate that it's no longer interested in the subscription by
  // dropping all its references to the hit test source & we need to make sure
  // that we don't keep the XRHitTestSources alive. HeapHashMap entries will
  // automatically be removed when the hit test sources get reclaimed, so we
  // need to maintain additional sets with their IDs to be able to clean them up
  // on the device - this is done in |hit_test_source_ids_| and
  // |hit_test_source_for_transient_input_ids_|.
  // For the specifics of HeapHashMap<Key, WeakMember<Value>> behavior, see:
  // https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md#weak-collections
  HeapHashMap<uint64_t, WeakMember<XRHitTestSource>>
      hit_test_source_ids_to_hit_test_sources_;
  HeapHashMap<uint64_t, WeakMember<XRTransientInputHitTestSource>>
      hit_test_source_ids_to_transient_input_hit_test_sources_;

  // The entries in the above hash sets will be automatically removed by garbage
  // collection once the application drops all references to them. To avoid
  // introducing pre-finalizers on hit test sources, store the set of IDs that
  // we know about. We will then subsequently cross-reference the sets with hash
  // maps and notify the device about hit test sources that are no longer alive.
  HashSet<uint64_t> hit_test_source_ids_;
  HashSet<uint64_t> hit_test_source_for_transient_input_ids_;

  Member<XRPlaneManager> plane_manager_;
  Member<XRDepthManager> depth_manager_;

  // Populated iff the raw camera feature has been enabled and the session
  // received a frame from the device that contained the camera image.
  absl::optional<gfx::Size> camera_image_size_;

  HeapVector<Member<XRViewData>> views_;
  Vector<device::mojom::blink::XRViewPtr> pending_views_;

  Member<XRInputSourceArray> input_sources_;
  Member<XRWebGLLayer> prev_base_layer_;
  Member<ResizeObserver> resize_observer_;
  Member<XRCanvasInputProvider> canvas_input_provider_;
  Member<Element> overlay_element_;
  Member<XRDOMOverlayState> dom_overlay_state_;
  bool environment_error_handler_subscribed_ = false;
  // Set of promises returned from requestHitTestSource and
  // requestHitTestSourceForTransientInput that are still in-flight.
  HeapHashSet<Member<ScriptPromiseResolver>> request_hit_test_source_promises_;
  HeapVector<Member<XRReferenceSpace>> reference_spaces_;

  uint32_t stage_parameters_id_ = 0;
  device::mojom::blink::VRStageParametersPtr stage_parameters_;

  HeapMojoReceiver<device::mojom::blink::XRSessionClient, XRSession>
      client_receiver_;
  HeapMojoAssociatedReceiver<device::mojom::blink::XRInputSourceButtonListener,
                             XRSession>
      input_receiver_;

  // Used to schedule video.rVFC callbacks for immersive sessions.
  Vector<ExecuteVfcCallback> vfc_execution_queue_;

  Member<XRFrameRequestCallbackCollection> callback_collection_;
  // Viewer pose in mojo space.
  std::unique_ptr<gfx::Transform> mojo_from_viewer_;

  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool frames_throttled_ = false;

  bool views_updated_this_frame_ = false;
  bool canvas_was_resized_ = false;

  // Indicates that we've already logged a metric, so don't need to log it
  // again.
  mutable bool did_log_getInputSources_ = false;
  mutable bool did_log_getViewerPose_ = false;

  // Dimensions of the output canvas.
  int output_width_ = 1;
  int output_height_ = 1;

  bool uses_input_eventing_ = false;
  float recommended_framebuffer_scale_ = 1.0;

  // Corresponds to mojo XRSession.supportsViewportScaling
  bool supports_viewport_scaling_ = false;

  // Corresponds to mojo XRSessionOptions.enable_anti_aliasing
  bool enable_anti_aliasing_ = true;

  std::unique_ptr<XRSessionViewportScaler> viewport_scaler_;

  // Indicates that this is a sensorless session which should only support the
  // identity reference space.
  bool sensorless_session_ = false;

  int16_t last_frame_id_ = -1;

  bool emulated_position_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
