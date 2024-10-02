// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/panner_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// A PannerNode only supports 1 or 2 channels
constexpr unsigned kMinimumOutputChannels = 1;
constexpr unsigned kMaximumOutputChannels = 2;

constexpr char kEqualPowerString[] = "equalpower";
constexpr char kHrtfString[] = "HRTF";

void FixNANs(double& x) {
  if (!std::isfinite(x)) {
    x = 0.0;
  }
}

}  // namespace

PannerHandler::PannerHandler(AudioNode& node,
                             float sample_rate,
                             AudioParamHandler& position_x,
                             AudioParamHandler& position_y,
                             AudioParamHandler& position_z,
                             AudioParamHandler& orientation_x,
                             AudioParamHandler& orientation_y,
                             AudioParamHandler& orientation_z)
    : AudioHandler(kNodeTypePanner, node, sample_rate),
      listener_(node.context()->listener()),
      position_x_(&position_x),
      position_y_(&position_y),
      position_z_(&position_z),
      orientation_x_(&orientation_x),
      orientation_y_(&orientation_y),
      orientation_z_(&orientation_z) {
  AddInput();
  AddOutput(kMaximumOutputChannels);

  // Node-specific default mixing rules.
  channel_count_ = kMaximumOutputChannels;
  SetInternalChannelCountMode(kClampedMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  // Explicitly set the default panning model here so that the histograms
  // include the default value.
  SetPanningModel(kEqualPowerString);

  Initialize();
}

scoped_refptr<PannerHandler> PannerHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& position_x,
    AudioParamHandler& position_y,
    AudioParamHandler& position_z,
    AudioParamHandler& orientation_x,
    AudioParamHandler& orientation_y,
    AudioParamHandler& orientation_z) {
  return base::AdoptRef(new PannerHandler(node, sample_rate, position_x,
                                          position_y, position_z, orientation_x,
                                          orientation_y, orientation_z));
}

PannerHandler::~PannerHandler() {
  Uninitialize();
}

// PannerNode needs a custom ProcessIfNecessary to get the process lock when
// computing PropagatesSilence() to protect processing from changes happening to
// the panning model.  This is very similar to AudioNode::ProcessIfNecessary.
void PannerHandler::ProcessIfNecessary(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  if (!IsInitialized()) {
    return;
  }

  // Ensure that we only process once per rendering quantum.
  // This handles the "fanout" problem where an output is connected to multiple
  // inputs.  The first time we're called during this time slice we process, but
  // after that we don't want to re-process, instead our output(s) will already
  // have the results cached in their bus
  const double current_time = Context()->currentTime();
  if (last_processing_time_ != current_time) {
    // important to first update this time because of feedback loops in the
    // rendering graph.
    last_processing_time_ = current_time;

    PullInputs(frames_to_process);

    const bool silent_inputs = InputsAreSilent();

    {
      // Need to protect calls to PropagatesSilence (and Process) because the
      // main thread may be changing the panning model that modifies the
      // TailTime and LatencyTime methods called by PropagatesSilence.
      base::AutoTryLock try_locker(process_lock_);
      if (try_locker.is_acquired()) {
        if (silent_inputs && PropagatesSilence()) {
          SilenceOutputs();
          // AudioParams still need to be processed so that the value can be
          // updated if there are automations or so that the upstream nodes get
          // pulled if any are connected to the AudioParam.
          ProcessOnlyAudioParams(frames_to_process);
        } else {
          // Unsilence the outputs first because the processing of the node may
          // cause the outputs to go silent and we want to propagate that hint
          // to the downstream nodes.  (For example, a Gain node with a gain of
          // 0 will want to silence its output.)
          UnsilenceOutputs();
          Process(frames_to_process);
        }
      } else {
        // We must be in the middle of changing the properties of the panner.
        // Just output silence.
        AudioBus* destination = Output(0).Bus();
        destination->Zero();
      }
    }

    if (!silent_inputs) {
      // Update `last_non_silent_time` AFTER processing this block.
      // Doing it before causes `PropagateSilence()` to be one render
      // quantum longer than necessary.
      last_non_silent_time_ =
          (Context()->CurrentSampleFrame() + frames_to_process) /
          static_cast<double>(Context()->sampleRate());
    }
  }
}

void PannerHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "PannerHandler::Process");

  AudioBus* destination = Output(0).Bus();

  if (!IsInitialized() || !panner_.get()) {
    destination->Zero();
    return;
  }

  scoped_refptr<AudioBus> source = Input(0).Bus();
  if (!source) {
    destination->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  auto listener = Listener();
  base::AutoTryLock try_listener_locker(listener->ListenerLock());

  if (try_listener_locker.is_acquired()) {
    if (!Context()->HasRealtimeConstraint() &&
        panning_model_ == Panner::PanningModel::kHRTF) {
      // For an OfflineAudioContext, we need to make sure the HRTFDatabase
      // is loaded before proceeding.  For realtime contexts, we don't
      // have to wait.  The HRTF panner handles that case itself.
      listener->WaitForHRTFDatabaseLoaderThreadCompletion();
    }

    if ((HasSampleAccurateValues() || listener->HasSampleAccurateValues()) &&
        (IsAudioRate() || listener->IsAudioRate())) {
      // It's tempting to skip sample-accurate processing if
      // isAzimuthElevationDirty() and isDistanceConeGain() both return false.
      // But in general we can't because something may scheduled to start in the
      // middle of the rendering quantum.  On the other hand, the audible effect
      // may be small enough that we can afford to do this optimization.
      ProcessSampleAccurateValues(destination, source.get(), frames_to_process);
    } else {
      // Apply the panning effect.
      double azimuth;
      double elevation;

      // Update dirty state in case something has moved; this can happen if the
      // AudioParam for the position or orientation component is set directly.
      UpdateDirtyState();

      AzimuthElevation(&azimuth, &elevation);

      panner_->Pan(azimuth, elevation, source.get(), destination,
                   frames_to_process, InternalChannelInterpretation());

      // Get the distance and cone gain.
      const float total_gain = DistanceConeGain();

      // Apply gain in-place.
      destination->CopyWithGainFrom(*destination, total_gain);
    }
  } else {
    // Too bad - The tryLock() failed.  We must be in the middle of changing the
    // properties of the panner or the listener.
    destination->Zero();
  }
}

void PannerHandler::ProcessSampleAccurateValues(AudioBus* destination,
                                                const AudioBus* source,
                                                uint32_t frames_to_process) {
  CHECK_LE(frames_to_process, GetDeferredTaskHandler().RenderQuantumFrames());

  // Get the sample accurate values from all of the AudioParams, including the
  // values from the AudioListener.
  float panner_x[GetDeferredTaskHandler().RenderQuantumFrames()];
  float panner_y[GetDeferredTaskHandler().RenderQuantumFrames()];
  float panner_z[GetDeferredTaskHandler().RenderQuantumFrames()];

  float orientation_x[GetDeferredTaskHandler().RenderQuantumFrames()];
  float orientation_y[GetDeferredTaskHandler().RenderQuantumFrames()];
  float orientation_z[GetDeferredTaskHandler().RenderQuantumFrames()];

  position_x_->CalculateSampleAccurateValues(panner_x, frames_to_process);
  position_y_->CalculateSampleAccurateValues(panner_y, frames_to_process);
  position_z_->CalculateSampleAccurateValues(panner_z, frames_to_process);
  orientation_x_->CalculateSampleAccurateValues(orientation_x,
                                                frames_to_process);
  orientation_y_->CalculateSampleAccurateValues(orientation_y,
                                                frames_to_process);
  orientation_z_->CalculateSampleAccurateValues(orientation_z,
                                                frames_to_process);

  // Get the automation values from the listener.
  auto listener = Listener();
  const float* listener_x = listener->GetPositionXValues(
      GetDeferredTaskHandler().RenderQuantumFrames());
  const float* listener_y = listener->GetPositionYValues(
      GetDeferredTaskHandler().RenderQuantumFrames());
  const float* listener_z = listener->GetPositionZValues(
      GetDeferredTaskHandler().RenderQuantumFrames());

  const float* forward_x = listener->GetForwardXValues(
      GetDeferredTaskHandler().RenderQuantumFrames());
  const float* forward_y = listener->GetForwardYValues(
      GetDeferredTaskHandler().RenderQuantumFrames());
  const float* forward_z = listener->GetForwardZValues(
      GetDeferredTaskHandler().RenderQuantumFrames());

  const float* up_x =
      listener->GetUpXValues(GetDeferredTaskHandler().RenderQuantumFrames());
  const float* up_y =
      listener->GetUpYValues(GetDeferredTaskHandler().RenderQuantumFrames());
  const float* up_z =
      listener->GetUpZValues(GetDeferredTaskHandler().RenderQuantumFrames());

  // Compute the azimuth, elevation, and total gains for each position.
  double azimuth[GetDeferredTaskHandler().RenderQuantumFrames()];
  double elevation[GetDeferredTaskHandler().RenderQuantumFrames()];
  float total_gain[GetDeferredTaskHandler().RenderQuantumFrames()];

  for (unsigned k = 0; k < frames_to_process; ++k) {
    gfx::Point3F panner_position(panner_x[k], panner_y[k], panner_z[k]);
    gfx::Vector3dF orientation(orientation_x[k], orientation_y[k],
                               orientation_z[k]);
    gfx::Point3F listener_position(listener_x[k], listener_y[k], listener_z[k]);
    gfx::Vector3dF listener_forward(forward_x[k], forward_y[k], forward_z[k]);
    gfx::Vector3dF listener_up(up_x[k], up_y[k], up_z[k]);

    CalculateAzimuthElevation(&azimuth[k], &elevation[k], panner_position,
                              listener_position, listener_forward, listener_up);

    // Get distance and cone gain
    total_gain[k] = CalculateDistanceConeGain(panner_position, orientation,
                                              listener_position);
  }

  // Update cached values in case automations end.
  if (frames_to_process > 0) {
    cached_azimuth_ = azimuth[frames_to_process - 1];
    cached_elevation_ = elevation[frames_to_process - 1];
    cached_distance_cone_gain_ = total_gain[frames_to_process - 1];
  }

  panner_->PanWithSampleAccurateValues(azimuth, elevation, source, destination,
                                       frames_to_process,
                                       InternalChannelInterpretation());
  destination->CopyWithSampleAccurateGainValuesFrom(*destination, total_gain,
                                                    frames_to_process);
}

void PannerHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  float values[GetDeferredTaskHandler().RenderQuantumFrames()];

  DCHECK_LE(frames_to_process, GetDeferredTaskHandler().RenderQuantumFrames());

  position_x_->CalculateSampleAccurateValues(values, frames_to_process);
  position_y_->CalculateSampleAccurateValues(values, frames_to_process);
  position_z_->CalculateSampleAccurateValues(values, frames_to_process);

  orientation_x_->CalculateSampleAccurateValues(values, frames_to_process);
  orientation_y_->CalculateSampleAccurateValues(values, frames_to_process);
  orientation_z_->CalculateSampleAccurateValues(values, frames_to_process);
}

void PannerHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  auto listener = Listener();
  panner_ = Panner::Create(panning_model_, Context()->sampleRate(),
                           GetDeferredTaskHandler().RenderQuantumFrames(),
                           listener->HrtfDatabaseLoader());
  listener->AddPanner(*this);

  // The panner is already marked as dirty, so `last_position_` and
  // `last_orientation_` will be updated on first use.  Don't need to
  // set them here.

  AudioHandler::Initialize();
}

void PannerHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  panner_.reset();
  auto listener = Listener();
  if (listener) {
    // Listener may have gone in the same garbage collection cycle, which means
    // that the panner does not need to be removed.
    listener->RemovePanner(*this);
  }

  AudioHandler::Uninitialize();
}

CrossThreadPersistent<AudioListener> PannerHandler::Listener() const {
  return listener_.Lock();
}

String PannerHandler::PanningModel() const {
  switch (panning_model_) {
    case Panner::PanningModel::kEqualPower:
      return kEqualPowerString;
    case Panner::PanningModel::kHRTF:
      return kHrtfString;
  }
  NOTREACHED();
  return kEqualPowerString;
}

void PannerHandler::SetPanningModel(const String& model) {
  // WebIDL should guarantee that we are never called with an invalid string
  // for the model.
  if (model == kEqualPowerString) {
    SetPanningModel(Panner::PanningModel::kEqualPower);
  } else if (model == kHrtfString) {
    SetPanningModel(Panner::PanningModel::kHRTF);
  } else {
    NOTREACHED();
  }
}

// This method should only be called from setPanningModel(const String&)!
bool PannerHandler::SetPanningModel(Panner::PanningModel model) {
  base::UmaHistogramEnumeration("WebAudio.PannerNode.PanningModel", model);

  if (model == Panner::PanningModel::kHRTF) {
    // Load the HRTF database asynchronously so we don't block the
    // Javascript thread while creating the HRTF database. It's ok to call
    // this multiple times; we won't be constantly loading the database over
    // and over.
    Listener()->CreateAndLoadHRTFDatabaseLoader(Context()->sampleRate());
  }

  if (!panner_.get() || model != panning_model_) {
    // We need the graph lock to secure the panner backend because
    // BaseAudioContext::Handle{Pre,Post}RenderTasks() from the audio thread
    // can touch it.
    BaseAudioContext::GraphAutoLocker context_locker(Context());

    // This synchronizes with process().
    base::AutoLock process_locker(process_lock_);
    panner_ = Panner::Create(model, Context()->sampleRate(),
                             GetDeferredTaskHandler().RenderQuantumFrames(),
                             Listener()->HrtfDatabaseLoader());
    panning_model_ = model;
  }
  return true;
}

String PannerHandler::DistanceModel() const {
  switch (const_cast<PannerHandler*>(this)->distance_effect_.Model()) {
    case DistanceEffect::kModelLinear:
      return "linear";
    case DistanceEffect::kModelInverse:
      return "inverse";
    case DistanceEffect::kModelExponential:
      return "exponential";
    default:
      NOTREACHED();
      return "inverse";
  }
}

void PannerHandler::SetDistanceModel(const String& model) {
  if (model == "linear") {
    SetDistanceModel(DistanceEffect::kModelLinear);
  } else if (model == "inverse") {
    SetDistanceModel(DistanceEffect::kModelInverse);
  } else if (model == "exponential") {
    SetDistanceModel(DistanceEffect::kModelExponential);
  }
}

bool PannerHandler::SetDistanceModel(unsigned model) {
  switch (model) {
    case DistanceEffect::kModelLinear:
    case DistanceEffect::kModelInverse:
    case DistanceEffect::kModelExponential:
      if (model != distance_model_) {
        // This synchronizes with process().
        base::AutoLock process_locker(process_lock_);
        distance_effect_.SetModel(
            static_cast<DistanceEffect::ModelType>(model));
        distance_model_ = model;
      }
      break;
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

void PannerHandler::SetRefDistance(double distance) {
  if (RefDistance() == distance) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  distance_effect_.SetRefDistance(distance);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetMaxDistance(double distance) {
  if (MaxDistance() == distance) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  distance_effect_.SetMaxDistance(distance);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetRolloffFactor(double factor) {
  if (RolloffFactor() == factor) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  distance_effect_.SetRolloffFactor(factor);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeInnerAngle(double angle) {
  if (ConeInnerAngle() == angle) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  cone_effect_.SetInnerAngle(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeOuterAngle(double angle) {
  if (ConeOuterAngle() == angle) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  cone_effect_.SetOuterAngle(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeOuterGain(double angle) {
  if (ConeOuterGain() == angle) {
    return;
  }

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  cone_effect_.SetOuterGain(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetPosition(float x,
                                float y,
                                float z,
                                ExceptionState& exceptionState) {
  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);

  double now = Context()->currentTime();

  position_x_->Timeline().SetValueAtTime(x, now, exceptionState);
  position_y_->Timeline().SetValueAtTime(y, now, exceptionState);
  position_z_->Timeline().SetValueAtTime(z, now, exceptionState);

  MarkPannerAsDirty(PannerHandler::kAzimuthElevationDirty |
                    PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetOrientation(float x,
                                   float y,
                                   float z,
                                   ExceptionState& exceptionState) {
  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);

  double now = Context()->currentTime();

  orientation_x_->Timeline().SetValueAtTime(x, now, exceptionState);
  orientation_y_->Timeline().SetValueAtTime(y, now, exceptionState);
  orientation_z_->Timeline().SetValueAtTime(z, now, exceptionState);

  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::CalculateAzimuthElevation(
    double* out_azimuth,
    double* out_elevation,
    const gfx::Point3F& position,
    const gfx::Point3F& listener_position,
    const gfx::Vector3dF& listener_forward,
    const gfx::Vector3dF& listener_up) {
  // Calculate the source-listener vector
  gfx::Vector3dF source_listener = position - listener_position;

  // Quick default return if the source and listener are at the same position.
  if (!source_listener.GetNormalized(&source_listener)) {
    *out_azimuth = 0;
    *out_elevation = 0;
    return;
  }

  // Align axes
  gfx::Vector3dF listener_right =
      gfx::CrossProduct(listener_forward, listener_up);
  listener_right.GetNormalized(&listener_right);

  gfx::Vector3dF listener_forward_norm = listener_forward;
  listener_forward_norm.GetNormalized(&listener_forward_norm);

  gfx::Vector3dF up = gfx::CrossProduct(listener_right, listener_forward_norm);

  float up_projection = gfx::DotProduct(source_listener, up);

  gfx::Vector3dF projected_source =
      source_listener - gfx::ScaleVector3d(up, up_projection);
  projected_source.GetNormalized(&projected_source);

  // Don't use gfx::AngleBetweenVectorsInDegrees here.  It produces the wrong
  // value when one of the vectors has zero length.  We know here that
  // `projected_source` and `listener_right` are "normalized", so the dot
  // product is good enough.
  double azimuth = Rad2deg(acos(
      ClampTo(gfx::DotProduct(projected_source, listener_right), -1.0f, 1.0f)));
  FixNANs(azimuth);  // avoid illegal values

  // Source  in front or behind the listener
  double front_back = gfx::DotProduct(projected_source, listener_forward_norm);
  if (front_back < 0.0) {
    azimuth = 360.0 - azimuth;
  }

  // Make azimuth relative to "front" and not "right" listener vector
  if ((azimuth >= 0.0) && (azimuth <= 270.0)) {
    azimuth = 90.0 - azimuth;
  } else {
    azimuth = 450.0 - azimuth;
  }

  // Elevation
  double elevation =
      90 - gfx::AngleBetweenVectorsInDegrees(source_listener, up);
  FixNANs(elevation);  // avoid illegal values

  if (elevation > 90.0) {
    elevation = 180.0 - elevation;
  } else if (elevation < -90.0) {
    elevation = -180.0 - elevation;
  }

  if (out_azimuth) {
    *out_azimuth = azimuth;
  }
  if (out_elevation) {
    *out_elevation = elevation;
  }
}

float PannerHandler::CalculateDistanceConeGain(
    const gfx::Point3F& position,
    const gfx::Vector3dF& orientation,
    const gfx::Point3F& listener_position) {
  double listener_distance = (position - listener_position).Length();
  double distance_gain = distance_effect_.Gain(listener_distance);
  double cone_gain =
      cone_effect_.Gain(position, orientation, listener_position);

  return static_cast<float>(distance_gain * cone_gain);
}

void PannerHandler::AzimuthElevation(double* out_azimuth,
                                     double* out_elevation) {
  DCHECK(Context()->IsAudioThread());

  auto listener = Listener();
  // Calculate new azimuth and elevation if the panner or the listener changed
  // position or orientation in any way.
  if (IsAzimuthElevationDirty() || listener->IsListenerDirty()) {
    CalculateAzimuthElevation(&cached_azimuth_, &cached_elevation_,
                              GetPosition(), listener->GetPosition(),
                              listener->Orientation(), listener->UpVector());
    is_azimuth_elevation_dirty_ = false;
  }

  *out_azimuth = cached_azimuth_;
  *out_elevation = cached_elevation_;
}

float PannerHandler::DistanceConeGain() {
  DCHECK(Context()->IsAudioThread());

  auto listener = Listener();
  // Calculate new distance and cone gain if the panner or the listener
  // changed position or orientation in any way.
  if (IsDistanceConeGainDirty() || listener->IsListenerDirty()) {
    cached_distance_cone_gain_ = CalculateDistanceConeGain(
        GetPosition(), Orientation(), listener->GetPosition());
    is_distance_cone_gain_dirty_ = false;
  }

  return cached_distance_cone_gain_;
}

void PannerHandler::MarkPannerAsDirty(unsigned dirty) {
  if (dirty & PannerHandler::kAzimuthElevationDirty) {
    is_azimuth_elevation_dirty_ = true;
  }

  if (dirty & PannerHandler::kDistanceConeGainDirty) {
    is_distance_cone_gain_dirty_ = true;
  }
}

void PannerHandler::SetChannelCount(unsigned channel_count,
                                    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  if (channel_count >= kMinimumOutputChannels &&
      channel_count <= kMaximumOutputChannels) {
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
            "channelCount", channel_count, kMinimumOutputChannels,
            ExceptionMessages::kInclusiveBound, kMaximumOutputChannels,
            ExceptionMessages::kInclusiveBound));
  }
}

void PannerHandler::SetChannelCountMode(const String& mode,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  ChannelCountMode old_mode = InternalChannelCountMode();

  if (mode == "clamped-max") {
    new_channel_count_mode_ = kClampedMax;
  } else if (mode == "explicit") {
    new_channel_count_mode_ = kExplicit;
  } else if (mode == "max") {
    // This is not supported for a PannerNode, which can only handle 1 or 2
    // channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Panner: 'max' is not allowed");
    new_channel_count_mode_ = old_mode;
  } else {
    // Do nothing for other invalid values.
    new_channel_count_mode_ = old_mode;
  }

  if (new_channel_count_mode_ != old_mode) {
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
  }
}

gfx::Point3F PannerHandler::GetPosition() const {
  auto x = position_x_->IsAudioRate() ? position_x_->FinalValue()
                                      : position_x_->Value();
  auto y = position_y_->IsAudioRate() ? position_y_->FinalValue()
                                      : position_y_->Value();
  auto z = position_z_->IsAudioRate() ? position_z_->FinalValue()
                                      : position_z_->Value();

  return gfx::Point3F(x, y, z);
}

gfx::Vector3dF PannerHandler::Orientation() const {
  auto x = orientation_x_->IsAudioRate() ? orientation_x_->FinalValue()
                                         : orientation_x_->Value();
  auto y = orientation_y_->IsAudioRate() ? orientation_y_->FinalValue()
                                         : orientation_y_->Value();
  auto z = orientation_z_->IsAudioRate() ? orientation_z_->FinalValue()
                                         : orientation_z_->Value();

  return gfx::Vector3dF(x, y, z);
}

bool PannerHandler::HasSampleAccurateValues() const {
  return position_x_->HasSampleAccurateValues() ||
         position_y_->HasSampleAccurateValues() ||
         position_z_->HasSampleAccurateValues() ||
         orientation_x_->HasSampleAccurateValues() ||
         orientation_y_->HasSampleAccurateValues() ||
         orientation_z_->HasSampleAccurateValues();
}

bool PannerHandler::IsAudioRate() const {
  return position_x_->IsAudioRate() || position_y_->IsAudioRate() ||
         position_z_->IsAudioRate() || orientation_x_->IsAudioRate() ||
         orientation_y_->IsAudioRate() || orientation_z_->IsAudioRate();
}

void PannerHandler::UpdateDirtyState() {
  DCHECK(Context()->IsAudioThread());

  gfx::Point3F current_position = GetPosition();
  gfx::Vector3dF current_orientation = Orientation();

  bool has_moved = current_position != last_position_ ||
                   current_orientation != last_orientation_;

  if (has_moved) {
    last_position_ = current_position;
    last_orientation_ = current_orientation;

    MarkPannerAsDirty(PannerHandler::kAzimuthElevationDirty |
                      PannerHandler::kDistanceConeGainDirty);
  }
}

bool PannerHandler::RequiresTailProcessing() const {
  // If there's no internal panner method set up yet, assume we require tail
  // processing in case the HRTF panner is set later, which does require tail
  // processing.
  return panner_ ? panner_->RequiresTailProcessing() : true;
}

}  // namespace blink
