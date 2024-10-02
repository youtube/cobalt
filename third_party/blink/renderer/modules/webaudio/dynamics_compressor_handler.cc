// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_handler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_dynamics_compressor_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Set output to stereo by default.
constexpr unsigned kDefaultNumberOfOutputChannels = 2;

}  // namespace

DynamicsCompressorHandler::DynamicsCompressorHandler(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release)
    : AudioHandler(kNodeTypeDynamicsCompressor, node, sample_rate),
      threshold_(&threshold),
      knee_(&knee),
      ratio_(&ratio),
      reduction_(0),
      attack_(&attack),
      release_(&release) {
  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  SetInternalChannelCountMode(kClampedMax);

  Initialize();
}

scoped_refptr<DynamicsCompressorHandler> DynamicsCompressorHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release) {
  return base::AdoptRef(new DynamicsCompressorHandler(
      node, sample_rate, threshold, knee, ratio, attack, release));
}

DynamicsCompressorHandler::~DynamicsCompressorHandler() {
  Uninitialize();
}

void DynamicsCompressorHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "DynamicsCompressorHandler::Process");
  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamThreshold,
                                          threshold_->FinalValue());
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamKnee,
                                          knee_->FinalValue());
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRatio,
                                          ratio_->FinalValue());
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamAttack,
                                          attack_->FinalValue());
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRelease,
                                          release_->FinalValue());

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();
  dynamics_compressor_->Process(input_bus.get(), output_bus, frames_to_process);

  reduction_.store(
      dynamics_compressor_->ParameterValue(DynamicsCompressor::kParamReduction),
      std::memory_order_relaxed);
}

void DynamicsCompressorHandler::ProcessOnlyAudioParams(
    uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());
  DCHECK_LE(frames_to_process, GetDeferredTaskHandler().RenderQuantumFrames());

  float values[GetDeferredTaskHandler().RenderQuantumFrames()];

  threshold_->CalculateSampleAccurateValues(values, frames_to_process);
  knee_->CalculateSampleAccurateValues(values, frames_to_process);
  ratio_->CalculateSampleAccurateValues(values, frames_to_process);
  attack_->CalculateSampleAccurateValues(values, frames_to_process);
  release_->CalculateSampleAccurateValues(values, frames_to_process);
}

void DynamicsCompressorHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  AudioHandler::Initialize();
  dynamics_compressor_ = std::make_unique<DynamicsCompressor>(
      Context()->sampleRate(), kDefaultNumberOfOutputChannels);
}

bool DynamicsCompressorHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double DynamicsCompressorHandler::TailTime() const {
  return dynamics_compressor_->TailTime();
}

double DynamicsCompressorHandler::LatencyTime() const {
  return dynamics_compressor_->LatencyTime();
}

void DynamicsCompressorHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // A DynamicsCompressorNode only supports 1 or 2 channels
  if (channel_count > 0 && channel_count <= 2) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (InternalChannelCountMode() != kMax) {
        UpdateChannelsForInputs();
      }
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "channelCount", channel_count, 1,
            ExceptionMessages::kInclusiveBound, 2,
            ExceptionMessages::kInclusiveBound));
  }
}

void DynamicsCompressorHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  ChannelCountMode old_mode = InternalChannelCountMode();

  if (mode == "clamped-max") {
    new_channel_count_mode_ = kClampedMax;
  } else if (mode == "explicit") {
    new_channel_count_mode_ = kExplicit;
  } else if (mode == "max") {
    // This is not supported for a DynamicsCompressorNode, which can
    // only handle 1 or 2 channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The provided value 'max' is not an "
                                      "allowed value for ChannelCountMode");
    new_channel_count_mode_ = old_mode;
  } else {
    // Do nothing for other invalid values.
    new_channel_count_mode_ = old_mode;
  }

  if (new_channel_count_mode_ != old_mode) {
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
  }
}

}  // namespace blink
