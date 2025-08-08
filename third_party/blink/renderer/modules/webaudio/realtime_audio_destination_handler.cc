// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_handler.h"

#include "base/feature_list.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/platform/audio/audio_destination.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

namespace blink {

namespace {

constexpr unsigned kDefaultNumberOfInputChannels = 2;

}  // namespace

scoped_refptr<RealtimeAudioDestinationHandler>
RealtimeAudioDestinationHandler::Create(
    AudioNode& node,
    const WebAudioSinkDescriptor& sink_descriptor,
    const WebAudioLatencyHint& latency_hint,
    absl::optional<float> sample_rate) {
  return base::AdoptRef(
      new RealtimeAudioDestinationHandler(node, sink_descriptor, latency_hint,
                                          sample_rate));
}

RealtimeAudioDestinationHandler::RealtimeAudioDestinationHandler(
    AudioNode& node,
    const WebAudioSinkDescriptor& sink_descriptor,
    const WebAudioLatencyHint& latency_hint,
    absl::optional<float> sample_rate)
    : AudioDestinationHandler(node),
      sink_descriptor_(sink_descriptor),
      latency_hint_(latency_hint),
      sample_rate_(sample_rate),
      allow_pulling_audio_graph_(false),
      task_runner_(Context()->GetExecutionContext()->GetTaskRunner(
          TaskType::kInternalMediaRealTime)) {
  // Node-specific default channel count and mixing rules.
  channel_count_ = kDefaultNumberOfInputChannels;
  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);
}

RealtimeAudioDestinationHandler::~RealtimeAudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

void RealtimeAudioDestinationHandler::Dispose() {
  Uninitialize();
  AudioDestinationHandler::Dispose();
}

void RealtimeAudioDestinationHandler::Initialize() {
  DCHECK(IsMainThread());

  CreatePlatformDestination();
  AudioHandler::Initialize();
}

void RealtimeAudioDestinationHandler::Uninitialize() {
  DCHECK(IsMainThread());

  // It is possible that the handler is already uninitialized.
  if (!IsInitialized()) {
    return;
  }

  StopPlatformDestination();
  AudioHandler::Uninitialize();
}

void RealtimeAudioDestinationHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1307461): Currently creating a platform destination requires
  // a valid frame/document. This assumption is incorrect.
  if (!blink::WebLocalFrame::FrameForCurrentContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot change channel count on a detached document.");
    return;
  }

  // The channelCount for the input to this node controls the actual number of
  // channels we send to the audio hardware. It can only be set if the number
  // is less than the number of hardware channels.
  if (channel_count > MaxChannelCount()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound, MaxChannelCount(),
            ExceptionMessages::kInclusiveBound));
    return;
  }

  uint32_t old_channel_count = ChannelCount();
  AudioHandler::SetChannelCount(channel_count, exception_state);

  // After the context is closed, changing channel count will be ignored
  // because it will trigger the recreation of the platform destination. This
  // in turn can activate the audio rendering thread.
  AudioContext* context = static_cast<AudioContext*>(Context());
  CHECK(context);
  if (context->ContextState() == AudioContext::kClosed ||
      ChannelCount() == old_channel_count ||
      exception_state.HadException()) {
    return;
  }

  // Stop, re-create and start the destination to apply the new channel count.
  StopPlatformDestination();
  CreatePlatformDestination();
  StartPlatformDestination();
}

void RealtimeAudioDestinationHandler::StartRendering() {
  DCHECK(IsMainThread());

  StartPlatformDestination();
}

void RealtimeAudioDestinationHandler::StopRendering() {
  DCHECK(IsMainThread());

  StopPlatformDestination();
}

void RealtimeAudioDestinationHandler::Pause() {
  DCHECK(IsMainThread());
  if (platform_destination_) {
    platform_destination_->Pause();
  }
}

void RealtimeAudioDestinationHandler::Resume() {
  DCHECK(IsMainThread());
  if (platform_destination_) {
    platform_destination_->Resume();
  }
}

void RealtimeAudioDestinationHandler::RestartRendering() {
  DCHECK(IsMainThread());

  StopRendering();
  StartRendering();
}

uint32_t RealtimeAudioDestinationHandler::MaxChannelCount() const {
  return platform_destination_->MaxChannelCount();
}

double RealtimeAudioDestinationHandler::SampleRate() const {
  // This can be accessed from both threads (main and audio), so it is
  // possible that `platform_destination_` is not fully functional when it
  // is accssed by the audio thread.
  return platform_destination_ ? platform_destination_->SampleRate() : 0;
}

void RealtimeAudioDestinationHandler::Render(
    AudioBus* destination_bus,
    uint32_t number_of_frames,
    const AudioIOPosition& output_position,
    const AudioCallbackMetric& metric) {
  TRACE_EVENT0("webaudio", "RealtimeAudioDestinationHandler::Render");

  // Denormals can seriously hurt performance of audio processing. This will
  // take care of all AudioNode processes within this scope.
  DenormalDisabler denormal_disabler;

  AudioContext* context = static_cast<AudioContext*>(Context());

  // A sanity check for the associated context, but this does not guarantee the
  // safe execution of the subsequence operations because the handler holds
  // the context as UntracedMember and it can go away anytime.
  DCHECK(context);
  if (!context) {
    return;
  }

  context->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();

  // If this node is not initialized yet, pass silence to the platform audio
  // destination. It is for the case where this node is in the middle of
  // tear-down process.
  if (!IsInitialized()) {
    destination_bus->Zero();
    return;
  }

  context->HandlePreRenderTasks(&output_position, &metric);

  // Only pull on the audio graph if we have not stopped the destination.  It
  // takes time for the destination to stop, but we want to stop pulling before
  // the destination has actually stopped.
  if (IsPullingAudioGraphAllowed()) {
    // Renders the graph by pulling all the inputs to this node. This will in
    // turn pull on their inputs, all the way backwards through the graph.
    scoped_refptr<AudioBus> rendered_bus =
        Input(0).Pull(destination_bus, number_of_frames);

    DCHECK(rendered_bus);
    if (!rendered_bus) {
      // AudioNodeInput might be in the middle of destruction. Then the internal
      // summing bus will return as nullptr. Then zero out the output.
      destination_bus->Zero();
    } else if (rendered_bus != destination_bus) {
      // In-place processing was not possible. Copy the rendered result to the
      // given `destination_bus` buffer.
      destination_bus->CopyFrom(*rendered_bus);
    }
  } else {
    destination_bus->Zero();
  }

  // Processes "automatic" nodes that are not connected to anything. This can
  // be done after copying because it does not affect the rendered result.
  context->GetDeferredTaskHandler().ProcessAutomaticPullNodes(number_of_frames);

  context->HandlePostRenderTasks();

  context->HandleAudibility(destination_bus);

  // Advances the current sample-frame.
  AdvanceCurrentSampleFrame(number_of_frames);

  context->UpdateWorkletGlobalScopeOnRenderingThread();

  SetDetectSilenceIfNecessary(
      context->GetDeferredTaskHandler().HasAutomaticPullNodes());
}

// A flag for using FakeAudioWorker when an AudioContext with "playback"
// latency outputs silence.
BASE_FEATURE(kUseFakeAudioWorkerForPlaybackLatency,
             "UseFakeAudioWorkerForPlaybackLatency",
             base::FEATURE_ENABLED_BY_DEFAULT);

void RealtimeAudioDestinationHandler::SetDetectSilenceIfNecessary(
    bool has_automatic_pull_nodes) {
  // Use a FakeAudioWorker for a silent AudioContext with playback latency only
  // when it is allowed by a command line flag.
  if (base::FeatureList::IsEnabled(kUseFakeAudioWorkerForPlaybackLatency)) {
    // For playback latency, relax the callback timing restriction so the
    // SilentSinkSuspender can fall back a FakeAudioWorker if necessary.
    if (latency_hint_.Category() == WebAudioLatencyHint::kCategoryPlayback) {
      DCHECK(is_detecting_silence_);
      return;
    }
  }

  // For other latency profiles (interactive, balanced, exact), use the
  // following heristics for the FakeAudioWorker activation after detecting
  // silence:
  // a) When there is no automatic pull nodes (APN) in the graph, or
  // b) When this destination node has one or more input connection.
  bool needs_silence_detection =
      !has_automatic_pull_nodes || Input(0).IsConnected();

  // Post a cross-thread task only when the detecting condition has changed.
  if (is_detecting_silence_ != needs_silence_detection) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RealtimeAudioDestinationHandler::SetDetectSilence,
                            AsWeakPtr(), needs_silence_detection));
    is_detecting_silence_ = needs_silence_detection;
  }
}

void RealtimeAudioDestinationHandler::SetDetectSilence(bool detect_silence) {
  DCHECK(IsMainThread());

  platform_destination_->SetDetectSilence(detect_silence);
}

uint32_t RealtimeAudioDestinationHandler::GetCallbackBufferSize() const {
  DCHECK(IsMainThread());
  DCHECK(IsInitialized());

  return platform_destination_->CallbackBufferSize();
}

int RealtimeAudioDestinationHandler::GetFramesPerBuffer() const {
  DCHECK(IsMainThread());
  DCHECK(IsInitialized());

  return platform_destination_ ? platform_destination_->FramesPerBuffer() : 0;
}

void RealtimeAudioDestinationHandler::CreatePlatformDestination() {
  DCHECK(IsMainThread());

  if (base::FeatureList::IsEnabled(features::kWebAudioSinkSelection)) {
    platform_destination_ = AudioDestination::Create(
        *this, sink_descriptor_, ChannelCount(), latency_hint_, sample_rate_,
        Context()->GetDeferredTaskHandler().RenderQuantumFrames());
  } else {
    WebAudioSinkDescriptor
        sink_descriptor(String(""), sink_descriptor_.Token());
    platform_destination_ = AudioDestination::Create(
        *this, sink_descriptor, ChannelCount(), latency_hint_, sample_rate_,
        Context()->GetDeferredTaskHandler().RenderQuantumFrames());
  }

  // if `sample_rate_` is nullopt, it is supposed to use the default device
  // sample rate. Update the internal sample rate for subsequent device change
  // request. See https://crbug.com/1424839.
  if (!sample_rate_.has_value()) {
    sample_rate_ = platform_destination_->SampleRate();
  }

  // TODO(crbug.com/991981): Can't query `GetCallbackBufferSize()` here because
  // creating the destination is not a synchronous process. When anything
  // touches the destination information between this call and
  // `StartPlatformDestination()` can lead to a crash.
  TRACE_EVENT0("webaudio",
               "RealtimeAudioDestinationHandler::CreatePlatformDestination");
}

void RealtimeAudioDestinationHandler::StartPlatformDestination() {
  TRACE_EVENT1("webaudio",
               "RealtimeAudioDestinationHandler::StartPlatformDestination",
               "sink information (when starting a new destination)",
               audio_utilities::GetSinkInfoForTracing(
                  sink_descriptor_, latency_hint_, MaxChannelCount(),
                  sample_rate_.has_value() ? sample_rate_.value() : -1,
                  GetCallbackBufferSize()));
  DCHECK(IsMainThread());

  if (platform_destination_->IsPlaying()) {
    return;
  }

  AudioWorklet* audio_worklet = Context()->audioWorklet();
  if (audio_worklet && audio_worklet->IsReady()) {
    // This task runner is only used to fire the audio render callback, so it
    // MUST not be throttled to avoid potential audio glitch.
    platform_destination_->StartWithWorkletTaskRunner(
        audio_worklet->GetMessagingProxy()
            ->GetBackingWorkerThread()
            ->GetTaskRunner(TaskType::kInternalMediaRealTime));
  } else {
    platform_destination_->Start();
  }

  // Allow the graph to be pulled once the destination actually starts
  // requesting data.
  EnablePullingAudioGraph();
}

void RealtimeAudioDestinationHandler::StopPlatformDestination() {
  DCHECK(IsMainThread());

  // Stop pulling on the graph, even if the destination is still requesting data
  // for a while. (It may take a bit of time for the destination to stop.)
  DisablePullingAudioGraph();

  if (platform_destination_->IsPlaying()) {
    platform_destination_->Stop();
  }
}

void RealtimeAudioDestinationHandler::PrepareTaskRunnerForWorklet() {
  DCHECK(IsMainThread());
  DCHECK_EQ(Context()->ContextState(), BaseAudioContext::kSuspended);
  DCHECK(Context()->audioWorklet());
  DCHECK(Context()->audioWorklet()->IsReady());

  platform_destination_->SetWorkletTaskRunner(
      Context()->audioWorklet()->GetMessagingProxy()
          ->GetBackingWorkerThread()
          ->GetTaskRunner(TaskType::kInternalMediaRealTime));
}

void RealtimeAudioDestinationHandler::SetSinkDescriptor(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::OutputDeviceStatusCB callback) {
  TRACE_EVENT1("webaudio", "RealtimeAudioDestinationHandler::SetSinkDescriptor",
               "sink information (when descriptor change requested)",
               audio_utilities::GetSinkInfoForTracing(
                  sink_descriptor, latency_hint_, MaxChannelCount(),
                  sample_rate_.has_value() ? sample_rate_.value() : -1,
                  GetCallbackBufferSize()));
  DCHECK(IsMainThread());

  // After the context is closed, `SetSinkDescriptor` request will be ignored
  // because it will trigger the recreation of the platform destination. This in
  // turn can activate the audio rendering thread.
  AudioContext* context = static_cast<AudioContext*>(Context());
  CHECK(context);
  if (context->ContextState() == AudioContext::kClosed) {
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    return;
  }

  // Create a pending AudioDestination to replace the current one.
  scoped_refptr<AudioDestination> pending_platform_destination =
      AudioDestination::Create(
          *this, sink_descriptor, ChannelCount(), latency_hint_, sample_rate_,
          Context()->GetDeferredTaskHandler().RenderQuantumFrames());

  // With this pending AudioDestination, create and initialize an underlying
  // sink in order to query the device status. If the status is OK, then replace
  // the `platform_destination_` with the pending_platform_destination.
  media::OutputDeviceStatus status =
      pending_platform_destination->CreateSinkAndGetDeviceStatus();
  if (status == media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK) {
    StopPlatformDestination();
    platform_destination_ = pending_platform_destination;
    sink_descriptor_ = sink_descriptor;
    StartPlatformDestination();
  }

  std::move(callback).Run(status);
}

}  // namespace blink
