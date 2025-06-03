// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_display_media_stream_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_supported_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_media_stream_constraints.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/input_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/navigator_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BASE_FEATURE(kEnumerateDevicesRequestAudioCapabilities,
             "EnumerateDevicesRequestAudioCapabilities",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"display-capture\" is disallowed by permission "
    "policy.";

class PromiseResolverCallbacks final : public UserMediaRequest::Callbacks {
 public:
  PromiseResolverCallbacks(
      UserMediaRequestType media_type,
      ScriptPromiseResolverWithTracker<UserMediaRequestResult>* resolver,
      base::OnceCallback<void(const String&, CaptureController*)>
          on_success_follow_up)
      : media_type_(media_type),
        resolver_(resolver),
        on_success_follow_up_(std::move(on_success_follow_up)) {}
  ~PromiseResolverCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams,
                 CaptureController* capture_controller) override {
    if (media_type_ == UserMediaRequestType::kAllScreensMedia) {
      OnSuccessGetAllScreensMedia(streams);
      return;
    }

    DCHECK_EQ(streams.size(), 1u);
    MediaStream* stream = streams[0];

    if (on_success_follow_up_) {
      // Only getDisplayMedia() calls set |on_success_follow_up_|.
      // Successful invocations of getDisplayMedia() always have exactly
      // one video track.
      //
      // Extension API calls that are followed by a getUserMedia() call with
      // chromeMediaSourceId are treated liked getDisplayMedia() calls.
      MediaStreamTrackVector video_tracks = stream->getVideoTracks();
      DCHECK_EQ(video_tracks.size(), 1u);
      if (capture_controller) {
        capture_controller->SetVideoTrack(video_tracks[0], stream->id().Utf8());
      }
    }

    // Resolve Promise<MediaStream> on a microtask.
    resolver_->Resolve(stream);

    // Enqueue the follow-up microtask, if any is intended.
    if (on_success_follow_up_) {
      std::move(on_success_follow_up_).Run(stream->id(), capture_controller);
    }
  }

  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error,
               CaptureController* capture_controller,
               UserMediaRequestResult result) override {
    if (capture_controller) {
      capture_controller->FinalizeFocusDecision();
    }
    resolver_->Reject(error, result);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

 private:
  void OnSuccessGetAllScreensMedia(const MediaStreamVector& streams) {
    DCHECK(!streams.empty());
    DCHECK_EQ(UserMediaRequestType::kAllScreensMedia, media_type_);
    resolver_->Resolve(streams);
  }

  const UserMediaRequestType media_type_;

  Member<ScriptPromiseResolverWithTracker<UserMediaRequestResult>> resolver_;
  base::OnceCallback<void(const String&, CaptureController*)>
      on_success_follow_up_;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisplayCapturePolicyResult {
  kDisallowed = 0,
  kAllowed = 1,
  kMaxValue = kAllowed
};

#if !BUILDFLAG(IS_ANDROID)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceTargetFunctionResult {
  kPromiseProduced = 0,
  kGenericError = 1,
  kInvalidContext = 2,
  kDuplicateCallBeforePromiseResolution = 3,
  kDuplicateCallAfterPromiseResolution = 4,
  kElementAndMediaDevicesNotInSameExecutionContext = 5,
  kMaxValue = kElementAndMediaDevicesNotInSameExecutionContext
};

void RecordUma(SubCaptureTarget::Type type,
               ProduceTargetFunctionResult result) {
  if (type == SubCaptureTarget::Type::kCropTarget) {
    base::UmaHistogramEnumeration(
        "Media.RegionCapture.ProduceCropTarget.Function.Result", result);
  } else if (type == SubCaptureTarget::Type::kRestrictionTarget) {
    base::UmaHistogramEnumeration(
        "Media.ElementCapture.ProduceTarget.Function.Result", result);
  } else {
    NOTREACHED();
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProduceTargetPromiseResult {
  kPromiseResolved = 0,
  kPromiseRejected = 1,
  kMaxValue = kPromiseRejected
};

void RecordUma(SubCaptureTarget::Type type, ProduceTargetPromiseResult result) {
  if (type == SubCaptureTarget::Type::kCropTarget) {
    base::UmaHistogramEnumeration(
        "Media.RegionCapture.ProduceCropTarget.Promise.Result", result);
  } else if (type == SubCaptureTarget::Type::kRestrictionTarget) {
    base::UmaHistogramEnumeration(
        "Media.ElementCapture.ProduceTarget.Promise.Result", result);
  } else {
    NOTREACHED();
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

// When `blink::features::kGetDisplayMediaRequiresUserActivation` is enabled,
// calls to `getDisplayMedia()` will require a transient user activation. This
// can be bypassed with the `ScreenCaptureWithoutGestureAllowedForOrigins`
// policy though.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayMediaTransientActivation {
  kPresent = 0,
  kMissing = 1,
  kMissingButFeatureDisabled = 2,
  kMissingButPolicyOverrides = 3,
  kMaxValue = kMissingButPolicyOverrides
};

void RecordUma(GetDisplayMediaTransientActivation activation) {
  base::UmaHistogramEnumeration(
      "Media.GetDisplayMedia.RequiresUserActivationResult", activation);
}

bool TransientActivationRequirementSatisfied(LocalDOMWindow* window) {
  DCHECK(window);

  LocalFrame* const frame = window->GetFrame();
  if (!frame) {
    return false;  // Err on the side of caution. Intentionally neglect UMA.
  }

  const Settings* const settings = frame->GetSettings();
  if (!settings) {
    return false;  // Err on the side of caution. Intentionally neglect UMA.
  }

  if (LocalFrame::HasTransientUserActivation(frame) ||
      (RuntimeEnabledFeatures::
           CapabilityDelegationDisplayCaptureRequestEnabled() &&
       window->IsDisplayCaptureRequestTokenActive())) {
    RecordUma(GetDisplayMediaTransientActivation::kPresent);
    return true;
  }

  if (!RuntimeEnabledFeatures::GetDisplayMediaRequiresUserActivationEnabled()) {
    RecordUma(GetDisplayMediaTransientActivation::kMissingButFeatureDisabled);
    return true;
  }

  if (!settings->GetRequireTransientActivationForGetDisplayMedia()) {
    RecordUma(GetDisplayMediaTransientActivation::kMissingButPolicyOverrides);
    return true;
  }

  RecordUma(GetDisplayMediaTransientActivation::kMissing);
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
bool IsExtensionScreenSharingFunctionCall(const MediaStreamConstraints* options,
                                          ExceptionState& exception_state) {
  DCHECK(!exception_state.HadException());

  if (!options) {
    return false;
  }

  const V8UnionBooleanOrMediaTrackConstraints* const video = options->video();
  if (!video || video->GetContentType() !=
                    V8UnionBooleanOrMediaTrackConstraints::ContentType::
                        kMediaTrackConstraints) {
    return false;
  }

  const MediaTrackConstraints* const constraints =
      video->GetAsMediaTrackConstraints();
  if (!constraints || !constraints->hasMandatory()) {
    return false;
  }

  const HashMap<String, String> map =
      blink::Dictionary(constraints->mandatory())
          .GetOwnPropertiesAsStringHashMap(exception_state);

  return !exception_state.HadException() && map.Contains("chromeMediaSourceId");
}
#endif

MediaStreamConstraints* ToMediaStreamConstraints(
    const UserMediaStreamConstraints* source,
    ExceptionState& exception_state) {
  DCHECK(source);
  DCHECK(!exception_state.HadException());

  MediaStreamConstraints* const constraints = MediaStreamConstraints::Create();

  if (source->hasAudio()) {
    constraints->setAudio(source->audio());
  }

  if (source->hasVideo()) {
    constraints->setVideo(source->video());
  }

#if !BUILDFLAG(IS_ANDROID)
  if (source->hasController()) {
    const bool is_screen_sharing =
        IsExtensionScreenSharingFunctionCall(constraints, exception_state);

    if (exception_state.HadException()) {
      return nullptr;
    }

    if (!is_screen_sharing) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "CaptureController supplied for a non-screen-capture call.");
      return nullptr;
    }

    constraints->setController(source->controller());
  }
#endif

  return constraints;
}

MediaStreamConstraints* ToMediaStreamConstraints(
    const DisplayMediaStreamOptions* source) {
  MediaStreamConstraints* const constraints = MediaStreamConstraints::Create();
  if (source->hasAudio()) {
    constraints->setAudio(source->audio());
  }
  if (source->hasVideo()) {
    constraints->setVideo(source->video());
  }
  if (source->hasPreferCurrentTab()) {
    constraints->setPreferCurrentTab(source->preferCurrentTab());
  }
  if (source->hasAutoSelectAllScreens()) {
    constraints->setAutoSelectAllScreens(source->autoSelectAllScreens());
  }
  if (source->hasController()) {
    constraints->setController(source->controller());
  }
  if (source->hasSelfBrowserSurface()) {
    constraints->setSelfBrowserSurface(source->selfBrowserSurface());
  }
  if (source->hasSystemAudio()) {
    constraints->setSystemAudio(source->systemAudio());
  }
  if (source->hasSurfaceSwitching()) {
    constraints->setSurfaceSwitching(source->surfaceSwitching());
  }
  if (source->hasMonitorTypeSurfaces()) {
    constraints->setMonitorTypeSurfaces(source->monitorTypeSurfaces());
  }
  return constraints;
}

}  // namespace

const char MediaDevices::kSupplementName[] = "MediaDevices";

MediaDevices* MediaDevices::mediaDevices(Navigator& navigator) {
  MediaDevices* supplement =
      Supplement<Navigator>::From<MediaDevices>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<MediaDevices>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

MediaDevices::MediaDevices(Navigator& navigator)
    : ActiveScriptWrappable<MediaDevices>({}),
      Supplement<Navigator>(navigator),
      ExecutionContextLifecycleObserver(navigator.DomWindow()),
      stopped_(false),
      dispatcher_host_(navigator.GetExecutionContext()),
      receiver_(this, navigator.DomWindow()) {}

MediaDevices::~MediaDevices() = default;

ScriptPromise MediaDevices::enumerateDevices(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateWebRTCMethodCount(RTCAPIName::kEnumerateDevices);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    return ScriptPromise();
  }

  auto* result_tracker = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<EnumerateDevicesResult>>(
      script_state, "Media.MediaDevices.EnumerateDevices", base::Seconds(4));
  const ScriptPromise promise = result_tracker->Promise();

  enumerate_device_requests_.insert(result_tracker);

  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  GetDispatcherHost(frame).EnumerateDevices(
      /*request_audio_input=*/true, /*request_video_input=*/true,
      /*request_audio_output=*/true,
      /*request_video_input_capabilities=*/true,
      /*request_audio_input_capabilities=*/
      base::FeatureList::IsEnabled(kEnumerateDevicesRequestAudioCapabilities),
      WTF::BindOnce(&MediaDevices::DevicesEnumerated, WrapPersistent(this),
                    WrapPersistent(result_tracker)));
  return promise;
}

MediaTrackSupportedConstraints* MediaDevices::getSupportedConstraints() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return MediaTrackSupportedConstraints::Create();
}

ScriptPromise MediaDevices::getUserMedia(
    ScriptState* script_state,
    const UserMediaStreamConstraints* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This timeout of base::Seconds(8) is an initial value and based on the data
  // in Media.MediaDevices.GetUserMedia.Latency, it should be iterated upon.
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult>>(
      script_state, "Media.MediaDevices.GetUserMedia", base::Seconds(8));

  DCHECK(options);  // Guaranteed by the default value in the IDL.
  DCHECK(!exception_state.HadException());

  MediaStreamConstraints* const constraints =
      ToMediaStreamConstraints(options, exception_state);
  if (!constraints) {
    DCHECK(exception_state.HadException());
    resolver->RecordResultAndLatency(
        UserMediaRequestResult::kInvalidConstraints);
    return ScriptPromise();
  }

  return SendUserMediaRequest(UserMediaRequestType::kUserMedia, resolver,
                              constraints, exception_state);
}

ScriptPromise MediaDevices::SendUserMediaRequest(
    UserMediaRequestType media_type,
    ScriptPromiseResolverWithTracker<UserMediaRequestResult>* resolver,
    const MediaStreamConstraints* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!exception_state.HadException());

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotSupportedError,
        "No media device client available; "
        "is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return ScriptPromise();
  }

  base::OnceCallback<void(const String&, CaptureController*)>
      on_success_follow_up;
#if !BUILDFLAG(IS_ANDROID)
  if (media_type == UserMediaRequestType::kDisplayMedia ||
      IsExtensionScreenSharingFunctionCall(options, exception_state)) {
    if (options->hasController()) {
      on_success_follow_up = WTF::BindOnce(
          &MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity,
          WrapWeakPersistent(this));
    } else {
      // TODO(crbug.com/1381949): Don't wait until the IPC round-trip and have
      // the browser process focus-switch upon starting the capture.
      on_success_follow_up =
          WTF::BindOnce(&MediaDevices::CloseFocusWindowOfOpportunity,
                        WrapWeakPersistent(this));
    }
  }

  if (exception_state.HadException()) {
    resolver->RecordResultAndLatency(
        UserMediaRequestResult::kInvalidConstraints);
    return ScriptPromise();
  }
#endif

  auto* callbacks = MakeGarbageCollected<PromiseResolverCallbacks>(
      media_type, resolver, std::move(on_success_follow_up));

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  UserMediaClient* user_media_client = UserMediaClient::From(window);
  constexpr IdentifiableSurface::Type surface_type =
      IdentifiableSurface::Type::kMediaDevices_GetUserMedia;
  IdentifiableSurface surface;
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(surface_type)) {
    surface = IdentifiableSurface::FromTypeAndToken(
        surface_type, TokenFromConstraints(options));
  }
  ScriptPromise promise = resolver->Promise();
  UserMediaRequest* request =
      UserMediaRequest::Create(window, user_media_client, media_type, options,
                               callbacks, exception_state, surface);
  if (!request) {
    DCHECK(exception_state.HadException());
    resolver->RecordResultAndLatency(
        UserMediaRequestResult::kInvalidConstraints);
    RecordIdentifiabilityMetric(
        surface, GetExecutionContext(),
        IdentifiabilityBenignStringToken(exception_state.Message()));
    return ScriptPromise();
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotSupportedError, error_message,
        UserMediaRequestResult::kInsecureContext);
    return ScriptPromise();
  }

#if !BUILDFLAG(IS_ANDROID)
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    window->ConsumeDisplayCaptureRequestToken();
  }
#endif

  request->Start();
  return promise;
}

ScriptPromise MediaDevices::getAllScreensMedia(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This timeout of base::Seconds(6) is an initial value and based on the data
  // in Media.MediaDevices.GetAllScreensMedia.Latency, it should be iterated
  // upon.
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult>>(
      script_state, "Media.MediaDevices.GetAllScreensMedia", base::Seconds(6));

  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "No media device client available; is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return ScriptPromise();
  }

  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  constraints->setVideo(
      MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(true));
  constraints->setAutoSelectAllScreens(true);
  return SendUserMediaRequest(UserMediaRequestType::kAllScreensMedia, resolver,
                              constraints, exception_state);
}

ScriptPromise MediaDevices::getDisplayMedia(
    ScriptState* script_state,
    const DisplayMediaStreamOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LocalDOMWindow* const window = DomWindow();
  // This timeout of base::Seconds(6) is an initial value and based on the data
  // in Media.MediaDevices.GetDisplayMedia.Latency, it should be iterated upon.
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolverWithTracker<UserMediaRequestResult>>(
      script_state, "Media.MediaDevices.GetDisplayMedia", base::Seconds(6));

  if (!window) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "No local DOM window; is this a detached window?",
        UserMediaRequestResult::kContextDestroyed);
    return ScriptPromise();
  }

  const bool capture_allowed_by_permissions_policy = window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kDisplayCapture,
      ReportOptions::kReportOnFailure);

  base::UmaHistogramEnumeration(
      "Media.Ui.GetDisplayMedia.DisplayCapturePolicyResult",
      capture_allowed_by_permissions_policy
          ? DisplayCapturePolicyResult::kAllowed
          : DisplayCapturePolicyResult::kDisallowed);

  if (!capture_allowed_by_permissions_policy) {
    resolver->RecordAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotAllowedError,
        kFeaturePolicyBlocked, UserMediaRequestResult::kNotAllowedError);
    return ScriptPromise();
  }

  if (!TransientActivationRequirementSatisfied(window)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "getDisplayMedia() requires transient activation (user gesture).");
    return ScriptPromise();
  }

  if (options->hasAutoSelectAllScreens() && options->autoSelectAllScreens()) {
    resolver->RecordAndThrowTypeError(
        exception_state,
        "The autoSelectAllScreens property is not allowed for usage with "
        "getDisplayMedia.",
        UserMediaRequestResult::kInvalidConstraints);
    return ScriptPromise();
  }

  if (CaptureController* const capture_controller =
          options->getControllerOr(nullptr)) {
    if (capture_controller->IsBound()) {
      resolver->RecordAndThrowDOMException(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "A CaptureController object may only be used with a single "
          "getDisplayMedia() invocation.",
          UserMediaRequestResult::kInvalidStateError);

      return ScriptPromise();
    }
    capture_controller->SetIsBound(true);
  }

  MediaStreamConstraints* const constraints = ToMediaStreamConstraints(options);
  if (!options->hasSelfBrowserSurface() &&
      (!options->hasPreferCurrentTab() || !options->preferCurrentTab())) {
    constraints->setSelfBrowserSurface("exclude");
  }

  if (options->hasPreferCurrentTab() && options->preferCurrentTab()) {
    UseCounter::Count(window,
                      WebFeature::kGetDisplayMediaWithPreferCurrentTabTrue);
  }

  return SendUserMediaRequest(UserMediaRequestType::kDisplayMedia, resolver,
                              constraints, exception_state);
}

void MediaDevices::setCaptureHandleConfig(ScriptState* script_state,
                                          const CaptureHandleConfig* config,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config->hasExposeOrigin());
  DCHECK(config->hasHandle());

  if (config->handle().length() > 1024) {
    exception_state.ThrowTypeError(
        "Handle length exceeds 1024 16-bit characters.");
    return;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Current frame is detached.");
    return;
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window || !window->GetFrame()) {
    return;
  }

  if (!window->GetFrame()->IsOutermostMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Can only be called from the top-level document.");
    return;
  }

  auto config_ptr = mojom::blink::CaptureHandleConfig::New();
  config_ptr->expose_origin = config->exposeOrigin();
  config_ptr->capture_handle = config->handle();
  if (config->permittedOrigins().size() == 1 &&
      config->permittedOrigins()[0] == "*") {
    config_ptr->all_origins_permitted = true;
  } else {
    config_ptr->all_origins_permitted = false;
    config_ptr->permitted_origins.reserve(config->permittedOrigins().size());
    for (const auto& permitted_origin : config->permittedOrigins()) {
      if (permitted_origin == "*") {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Wildcard only valid in isolation.");
        return;
      }

      scoped_refptr<SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(permitted_origin);
      if (!origin || origin->IsOpaque()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Invalid origin encountered.");
        return;
      }
      config_ptr->permitted_origins.emplace_back(std::move(origin));
    }
  }

  GetDispatcherHost(window->GetFrame())
      .SetCaptureHandleConfig(std::move(config_ptr));
}

ScriptPromise MediaDevices::ProduceSubCaptureTarget(
    ScriptState* script_state,
    Element* element,
    ExceptionState& exception_state,
    SubCaptureTarget::Type type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(type == SubCaptureTarget::Type::kCropTarget ||
        type == SubCaptureTarget::Type::kRestrictionTarget);

#if BUILDFLAG(IS_ANDROID)
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported.");
  return ScriptPromise();
#else
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current frame is detached.");
    RecordUma(type, ProduceTargetFunctionResult::kInvalidContext);
    return ScriptPromise();
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    RecordUma(type, ProduceTargetFunctionResult::kGenericError);
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing execution context.");
    return ScriptPromise();
  }

  if (!element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid element.");
    return ScriptPromise();
  }

  if (GetExecutionContext() != element->GetExecutionContext()) {
    RecordUma(type, ProduceTargetFunctionResult::
                        kElementAndMediaDevicesNotInSameExecutionContext);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The Element and the MediaDevices object must be same-window.");
    return ScriptPromise();
  }

  if ((type == SubCaptureTarget::Type::kCropTarget &&
       element->GetRegionCaptureCropId()) ||
      (type == SubCaptureTarget::Type::kRestrictionTarget &&
       element->GetRestrictionTargetId())) {
    // A token was produced earlier and associated with the Element.
    const base::Token token = (type == SubCaptureTarget::Type::kCropTarget)
                                  ? element->GetRegionCaptureCropId()->value()
                                  : element->GetRestrictionTargetId()->value();
    DCHECK(!token.is_zero());
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_state.GetContext());
    const ScriptPromise promise = resolver->Promise();
    const WTF::String token_str(blink::TokenToGUID(token).AsLowercaseString());
    if (type == SubCaptureTarget::Type::kCropTarget) {
      resolver->Resolve(MakeGarbageCollected<CropTarget>(token_str));
    } else {  // kRestrictionTarget
      resolver->Resolve(MakeGarbageCollected<RestrictionTarget>(token_str));
    }
    RecordUma(
        type,
        ProduceTargetFunctionResult::kDuplicateCallAfterPromiseResolution);
    return promise;
  }

  ElementToResolverMap& map = GetResolverMap(type);
  const auto it = map.find(element);
  if (it != map.end()) {
    // The Element does not yet have the SubCaptureTarget attached,
    // but the production of one has already been kicked off, and a response
    // will soon arrive from the browser process.
    // The Promise we return here will be resolved along with the original one.
    RecordUma(
        type,
        ProduceTargetFunctionResult::kDuplicateCallBeforePromiseResolution);
    return it->value->Promise();
  }

  // Mints a new ID on the browser process.
  // Resolves after it has been produced and is ready to be used.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  map.insert(element, resolver);
  const ScriptPromise promise = resolver->Promise();

  base::OnceCallback callback =
      WTF::BindOnce(&MediaDevices::ResolveSubCaptureTargetPromise,
                    WrapPersistent(this), WrapPersistent(element), type);
  GetDispatcherHost(window->GetFrame())
      .ProduceSubCaptureTargetId(type, std::move(callback));

  RecordUma(type, ProduceTargetFunctionResult::kPromiseProduced);
  return promise;
#endif
}

const AtomicString& MediaDevices::InterfaceName() const {
  return event_target_names::kMediaDevices;
}

ExecutionContext* MediaDevices::GetExecutionContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void MediaDevices::RemoveAllEventListeners() {
  EventTarget::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void MediaDevices::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EventTarget::AddedEventListener(event_type, registered_listener);
  StartObserving();
}

void MediaDevices::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners()) {
    StopObserving();
  }
}

bool MediaDevices::HasPendingActivity() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return receiver_.is_bound();
}

void MediaDevices::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    return;
  }

  stopped_ = true;
  enumerate_device_requests_.clear();
}

void MediaDevices::OnDevicesChanged(
    mojom::blink::MediaDeviceType type,
    const Vector<WebMediaDeviceInfo>& device_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetExecutionContext());
  if (current_device_infos_[static_cast<wtf_size_t>(type)] == device_infos) {
    return;
  }

  current_device_infos_[static_cast<wtf_size_t>(type)] = device_infos;
  if (RuntimeEnabledFeatures::OnDeviceChangeEnabled()) {
    ScheduleDispatchEvent(Event::Create(event_type_names::kDevicechange));
  }
}

void MediaDevices::ScheduleDispatchEvent(Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduled_events_.push_back(event);
  if (dispatch_scheduled_events_task_handle_.IsActive()) {
    return;
  }

  auto* context = GetExecutionContext();
  DCHECK(context);
  dispatch_scheduled_events_task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      WTF::BindOnce(&MediaDevices::DispatchScheduledEvents,
                    WrapPersistent(this)));
}

void MediaDevices::DispatchScheduledEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    return;
  }
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events) {
    DispatchEvent(*event);
  }
}

void MediaDevices::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (receiver_.is_bound() || stopped_ || starting_observation_) {
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return;
  }

  starting_observation_ = true;
  GetDispatcherHost(window->GetFrame())
      .EnumerateDevices(/*request_audio_input=*/true,
                        /*request_video_input=*/true,
                        /*request_audio_output=*/true,
                        /*request_video_input_capabilities=*/false,
                        /*request_audio_input_capabilities=*/false,
                        WTF::BindOnce(&MediaDevices::FinalizeStartObserving,
                                      WrapPersistent(this)));
}

void MediaDevices::FinalizeStartObserving(
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  starting_observation_ = false;
  if (receiver_.is_bound() || stopped_) {
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return;
  }

  current_device_infos_ = enumeration;

  GetDispatcherHost(window->GetFrame())
      .AddMediaDevicesListener(true /* audio input */, true /* video input */,
                               true /* audio output */,
                               receiver_.BindNewPipeAndPassRemote(
                                   GetExecutionContext()->GetTaskRunner(
                                       TaskType::kMediaElementEvent)));
}

void MediaDevices::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_.is_bound()) {
    return;
  }
  receiver_.reset();
}

namespace {

void RecordEnumeratedDevices(ScriptState* script_state,
                             const MediaDeviceInfoVector& media_devices) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleWebFeature(
          WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices)) {
    return;
  }
  Document* document =
      LocalDOMWindow::From(script_state)->GetFrame()->GetDocument();
  IdentifiableTokenBuilder builder;
  for (const auto& device_info : media_devices) {
    // Ignore device_id since that varies per-site.
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->kind()));
    builder.AddToken(IdentifiabilityBenignStringToken(device_info->label()));
    // Ignore group_id since that is varies per-site.
  }
  IdentifiabilityMetricBuilder(document->UkmSourceID())
      .AddWebFeature(WebFeature::kIdentifiabilityMediaDevicesEnumerateDevices,
                     builder.GetToken())
      .Record(document->UkmRecorder());
}

}  // namespace

void MediaDevices::DevicesEnumerated(
    ScriptPromiseResolverWithTracker<EnumerateDevicesResult>* result_tracker,
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enumerate_device_requests_.Contains(result_tracker)) {
    return;
  }

  enumerate_device_requests_.erase(result_tracker);

  ScriptState* script_state = result_tracker->GetScriptState();
  if (!script_state || !ExecutionContext::From(script_state) ||
      ExecutionContext::From(script_state)->IsContextDestroyed()) {
    return;
  }

  DCHECK_EQ(static_cast<wtf_size_t>(
                mojom::blink::MediaDeviceType::kNumMediaDeviceTypes),
            enumeration.size());

  if (!video_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::kMediaVideoInput)]
                  .size(),
              video_input_capabilities.size());
  }
  if (!audio_input_capabilities.empty()) {
    DCHECK_EQ(enumeration[static_cast<wtf_size_t>(
                              mojom::blink::MediaDeviceType::kMediaAudioInput)]
                  .size(),
              audio_input_capabilities.size());
  }

  MediaDeviceInfoVector media_devices;
  for (wtf_size_t i = 0;
       i < static_cast<wtf_size_t>(
               mojom::blink::MediaDeviceType::kNumMediaDeviceTypes);
       ++i) {
    for (wtf_size_t j = 0; j < enumeration[i].size(); ++j) {
      mojom::blink::MediaDeviceType device_type =
          static_cast<mojom::blink::MediaDeviceType>(i);
      WebMediaDeviceInfo device_info = enumeration[i][j];
      String device_label = String::FromUTF8(device_info.label);
      if (device_type == mojom::blink::MediaDeviceType::kMediaAudioInput ||
          device_type == mojom::blink::MediaDeviceType::kMediaVideoInput) {
        InputDeviceInfo* input_device_info =
            MakeGarbageCollected<InputDeviceInfo>(
                String::FromUTF8(device_info.device_id), device_label,
                String::FromUTF8(device_info.group_id), device_type);
        if (device_type == mojom::blink::MediaDeviceType::kMediaVideoInput &&
            !video_input_capabilities.empty()) {
          input_device_info->SetVideoInputCapabilities(
              std::move(video_input_capabilities[j]));
        }
        if (device_type == mojom::blink::MediaDeviceType::kMediaAudioInput &&
            !audio_input_capabilities.empty()) {
          input_device_info->SetAudioInputCapabilities(
              std::move(audio_input_capabilities[j]));
        }
        media_devices.push_back(input_device_info);
      } else {
        media_devices.push_back(MakeGarbageCollected<MediaDeviceInfo>(
            String::FromUTF8(device_info.device_id), device_label,
            String::FromUTF8(device_info.group_id), device_type));
      }
    }
  }

  RecordEnumeratedDevices(result_tracker->GetScriptState(), media_devices);
  result_tracker->Resolve(media_devices);
}

void MediaDevices::OnDispatcherHostConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (ScriptPromiseResolverWithTracker<EnumerateDevicesResult>*
           result_tracker : enumerate_device_requests_) {
    result_tracker->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "enumerateDevices() failed."),
        EnumerateDevicesResult::kErrorMediaDevicesDispatcherHostDisconnected);
  }
  enumerate_device_requests_.clear();
  dispatcher_host_.reset();
}

mojom::blink::MediaDevicesDispatcherHost& MediaDevices::GetDispatcherHost(
    LocalFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!dispatcher_host_.is_bound()) {
    // Note: kInternalMediaRealTime is a better candidate for this job,
    // but kMediaElementEvent is used for consistency.
    frame->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMediaElementEvent)));
    dispatcher_host_.set_disconnect_handler(
        WTF::BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
                      WrapWeakPersistent(this)));
  }

  DCHECK(dispatcher_host_.get());
  return *dispatcher_host_.get();
}

void MediaDevices::SetDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>
        dispatcher_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const execution_context = GetExecutionContext();
  DCHECK(execution_context);

  dispatcher_host_.Bind(
      std::move(dispatcher_host),
      execution_context->GetTaskRunner(TaskType::kMediaElementEvent));
  dispatcher_host_.set_disconnect_handler(
      WTF::BindOnce(&MediaDevices::OnDispatcherHostConnectionError,
                    WrapWeakPersistent(this)));
}

void MediaDevices::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_host_);
  visitor->Trace(receiver_);
  visitor->Trace(scheduled_events_);
  visitor->Trace(enumerate_device_requests_);
#if !BUILDFLAG(IS_ANDROID)
  visitor->Trace(crop_target_resolvers_);
  visitor->Trace(restriction_target_resolvers_);
#endif
  Supplement<Navigator>::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

#if !BUILDFLAG(IS_ANDROID)
MediaDevices::ElementToResolverMap& MediaDevices::GetResolverMap(
    SubCaptureTarget::Type type) {
  switch (type) {
    case SubCaptureTarget::Type::kCropTarget:
      return crop_target_resolvers_;
    case SubCaptureTarget::Type::kRestrictionTarget:
      return restriction_target_resolvers_;
  }
  NOTREACHED_NORETURN();
}

void MediaDevices::EnqueueMicrotaskToCloseFocusWindowOfOpportunity(
    const String& id,
    CaptureController* capture_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return;
  }

  context->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      &MediaDevices::CloseFocusWindowOfOpportunity, WrapWeakPersistent(this),
      id, WrapWeakPersistent(capture_controller)));
}

void MediaDevices::CloseFocusWindowOfOpportunity(
    const String& id,
    CaptureController* capture_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return;  // Note: We're still back by the browser-side timer.
  }

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  if (!window) {
    return;
  }

  if (capture_controller) {
    capture_controller->FinalizeFocusDecision();
  }

  GetDispatcherHost(window->GetFrame()).CloseFocusWindowOfOpportunity(id);
}

// An empty |id| signals failure; anything else has to be a valid GUID
// and signals success.
void MediaDevices::ResolveSubCaptureTargetPromise(Element* element,
                                                  SubCaptureTarget::Type type,
                                                  const WTF::String& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(type == SubCaptureTarget::Type::kCropTarget ||
        type == SubCaptureTarget::Type::kRestrictionTarget);
  CHECK(element);  // Persistent.

  ElementToResolverMap& map = GetResolverMap(type);
  const auto it = map.find(element);
  DCHECK_NE(it, map.end());
  ScriptPromiseResolver* const resolver = it->value;
  map.erase(it);

  if (id.empty()) {
    resolver->Reject();
    RecordUma(type, ProduceTargetPromiseResult::kPromiseRejected);
    return;
  }

  const base::Uuid guid = base::Uuid::ParseLowercase(id.Ascii());
  DCHECK(guid.is_valid());
  if (type == SubCaptureTarget::Type::kCropTarget) {
    element->SetRegionCaptureCropId(
        std::make_unique<RegionCaptureCropId>(blink::GUIDToToken(guid)));
    resolver->Resolve(MakeGarbageCollected<CropTarget>(id));
  } else {  // kRestrictionTarget as per earlier CHECK.
    element->SetRestrictionTargetId(
        std::make_unique<RestrictionTargetId>(blink::GUIDToToken(guid)));
    resolver->Resolve(MakeGarbageCollected<RestrictionTarget>(id));
  }
  RecordUma(type, ProduceTargetPromiseResult::kPromiseResolved);
}
#endif

}  // namespace blink
