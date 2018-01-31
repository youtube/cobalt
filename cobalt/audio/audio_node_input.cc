// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/audio/audio_node_input.h"

#include "base/logging.h"
#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/audio/audio_node_output.h"

namespace cobalt {
namespace audio {

namespace {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

void MixAudioBufferBasedOnInterpretation(
    const float* speaker, const float* discrete,
    const AudioNodeChannelInterpretation& interpretation, ShellAudioBus* source,
    ShellAudioBus* output_audio_data) {
  const float* kMatrix =
      interpretation == kAudioNodeChannelInterpretationSpeakers ? speaker
                                                                : discrete;
  size_t array_size = source->channels() * output_audio_data->channels();
  std::vector<float> matrix(kMatrix, kMatrix + array_size);
  output_audio_data->Mix(*source, matrix);
}

// "discrete" channel interpretation: up-mix by filling channels until they run
// out then zero out remaining channels. Down-mix by filling as many channels as
// possible, then dropping remaining channels.
// "speakers" channel interpretation: use the below spec. In cases where the
// number of channels do not match any of these basic speaker layouts, revert to
// "discrete".
// Up down mix equations for mono, stereo, quad, 5.1:
//   https://www.w3.org/TR/webaudio/#ChannelLayouts
void MixAudioBuffer(const AudioNodeChannelInterpretation& interpretation,
                    ShellAudioBus* source, ShellAudioBus* output_audio_data) {
  DCHECK_GT(source->channels(), 0u);
  DCHECK_GT(output_audio_data->channels(), 0u);
  DCHECK(interpretation == kAudioNodeChannelInterpretationSpeakers ||
         interpretation == kAudioNodeChannelInterpretationDiscrete)
      << interpretation;

  if (output_audio_data->channels() == source->channels()) {
    output_audio_data->Mix(*source);
  } else if (source->channels() == 1 && output_audio_data->channels() == 2) {
    // 1 -> 2: up-mix from mono to stereo.
    //
    // output.L = input;
    // output.R = input;
    const float kMonoToStereoMatrixSpeaker[] = {
        1.0f,  // 1.0 * input
        1.0f,  // 1.0 * input
    };

    const float kMonoToStereoMatrixDiscrete[] = {
        1.0f,  // 1.0 * input
        0.0f,  // 0.0 * input
    };

    MixAudioBufferBasedOnInterpretation(
        kMonoToStereoMatrixSpeaker, kMonoToStereoMatrixDiscrete, interpretation,
        source, output_audio_data);
  } else if (source->channels() == 4 && output_audio_data->channels() == 2) {
    // 4 -> 2: down-mix from quad to stereo.
    //
    // output.L = 0.5 * (input.L + input.SL);
    // output.R = 0.5 * (input.R + input.SR);
    const float kQuadToStereoMatrixSpeaker[] = {
        0.5f, 0.0f, 0.5f, 0.0f,  // 0.5 * L + 0.0 * R + 0.5 * SL + 0.0 * SR
        0.0f, 0.5f, 0.0f, 0.5f,  // 0.0 * L + 0.5 * R + 0.0 * SL + 0.5 * SR
    };

    const float kQuadToStereoMatrixDiscrete[] = {
        1.0f, 0.0f, 0.0f, 0.0f,  // 1.0 * L + 0.0 * R + 0.0 * SL + 0.0 * SR
        0.0f, 1.0f, 0.0f, 0.0f,  // 0.0 * L + 1.0 * R + 0.0 * SL + 0.0 * SR
    };

    MixAudioBufferBasedOnInterpretation(
        kQuadToStereoMatrixSpeaker, kQuadToStereoMatrixDiscrete, interpretation,
        source, output_audio_data);
  } else if (source->channels() == 6 && output_audio_data->channels() == 2) {
    // 5.1 -> 2: down-mix from 5.1 to stereo.
    //
    // output.L = L + 0.7071 * (input.C + input.SL)
    // output.R = R + 0.7071 * (input.C + input.SR)
    const float kFivePointOneToStereoMatrixSpeaker[] = {
        // 1.0 * L + 0.0 * R + 0.7071 * C + 0.0 * LFE + 0.7071 * SL + 0.0 * SR
        1.0f, 0.0f, 0.7071f, 0.0f, 0.7071f, 0.0f,
        // 0.0 * L + 1.0 * R + 0.7071 * C + 0.0 * LFE + 0.0 * SL + 0.7071 * SR
        0.0f, 1.0f, 0.7071f, 0.0f, 0.0f, 0.7071f,
    };

    const float kFivePointOneToStereoMatrixDiscrete[] = {
        // 1.0 * L + 0.0 * R + 0.0 * C + 0.0 * LFE + 0.0 * SL + 0.0 * SR
        1.f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        // 0.0 * L + 1.0 * R + 0.0 * C + 0.0 * LFE + 0.0 * SL + 0.0 * SR
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };

    MixAudioBufferBasedOnInterpretation(
        kFivePointOneToStereoMatrixSpeaker, kFivePointOneToStereoMatrixDiscrete,
        interpretation, source, output_audio_data);
  } else if (source->channels() == 2 && output_audio_data->channels() == 1) {
    // 2 -> 1: down-mix from stereo to mono.
    //
    // output = 0.5 * (input.L + input.R);
    const float kStereoToMonoSpeaker[] = {
        0.5f, 0.5f,  // 0.5 * L + 0.5 * R
    };

    const float kStereoToMonoDiscrete[] = {
        1.0f, 0.0f,  // 1.0 * L + 0.0 * R
    };

    MixAudioBufferBasedOnInterpretation(kStereoToMonoSpeaker,
                                        kStereoToMonoDiscrete, interpretation,
                                        source, output_audio_data);
  } else if (source->channels() == 4 && output_audio_data->channels() == 1) {
    // 4 -> 1: down-mix from quad to mono.
    //
    // output = 0.25 * (input.L + input.R + input.SL + input.SR);
    const float kQuadToMonoSpeaker[] = {
        // 0.25 * L + 0.25 * R + 0.25 * SL + 0.25 * SR
        0.25f, 0.25f, 0.25f, 0.25f,
    };

    const float kQuadToMonoDiscrete[] = {
        // 1.0 * L + 0.0 * R + 0.0 * SL + 0.0 * SR
        1.0f, 0.0f, 0.0f, 0.0f,
    };

    MixAudioBufferBasedOnInterpretation(kQuadToMonoSpeaker, kQuadToMonoDiscrete,
                                        interpretation, source,
                                        output_audio_data);
  } else if (source->channels() == 6 && output_audio_data->channels() == 1) {
    // 5.1 -> 1: down-mix from 5.1 to mono.
    //
    // output = 0.7071 * (input.L + input.R) + input.C + 0.5 * (input.SL +
    // input.SR)
    const float kFivePointOneToMonoSpeaker[] = {
        // 0.7071 * L + 0.7071 * R + 1.0 * C + 0.0 * LFE + 0.5 * SL + 0.5 * SR
        0.7071f, 0.7071f, 1.0f, 0.0f, 0.5f, 0.5f,
    };

    const float kFivePointOneToMonoDiscrete[] = {
        // 1.0 * L + 0.0 * R + 0.0 * C + 0.0 * LFE + 0.0 * SL + 0.0 * SR
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };

    MixAudioBufferBasedOnInterpretation(
        kFivePointOneToMonoSpeaker, kFivePointOneToMonoDiscrete, interpretation,
        source, output_audio_data);
  } else {
    // TODO: Implement the case which the number of channels do not
    // match any of those basic speaker layouts. In this case, use "discrete"
    // channel layout.
    NOTREACHED() << "The combination of source channels: " << source->channels()
                 << " and output channels: " << output_audio_data->channels()
                 << " is not supported.";
  }
}

}  // namespace

AudioNodeInput::~AudioNodeInput() {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(outputs_.empty());
}

void AudioNodeInput::Connect(AudioNodeOutput* output) {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(output);

  // There can only be one connection between a given output of one specific
  // node and a given input of another specific node. Multiple connections with
  // the same termini are ignored.
  if (outputs_.find(output) != outputs_.end()) {
    return;
  }

  output->AddInput(this);
  outputs_.insert(output);
  owner_node_->OnInputNodeConnected();
}

void AudioNodeInput::Disconnect(AudioNodeOutput* output) {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(output);

  if (outputs_.find(output) == outputs_.end()) {
    NOTREACHED();
    return;
  }

  outputs_.erase(output);
  output->RemoveInput(this);
}

void AudioNodeInput::DisconnectAll() {
  owner_node_->audio_lock()->AssertLocked();

  while (!outputs_.empty()) {
    AudioNodeOutput* output = *outputs_.begin();
    Disconnect(output);
  }
}

void AudioNodeInput::FillAudioBus(ShellAudioBus* output_audio_bus,
                                  bool* silence) {
  // This is called by Audio thread.
  owner_node_->audio_lock()->AssertLocked();

  // TODO: Consider computing computedNumberOfChannels and do up-mix or
  // down-mix base on computedNumberOfChannels. The current implementation
  // is based on the fact that the channelCountMode is max.
  if (owner_node_->channel_count_mode() != kAudioNodeChannelCountModeMax) {
    DLOG(ERROR) << "Unsupported channel count mode: "
                << owner_node_->channel_count_mode();
    return;
  }

  // Pull audio buffer from connected audio input. When an input is connected
  // from one or more AudioNode outputs. Fan-in is supported.
  for (std::set<AudioNodeOutput*>::iterator iter = outputs_.begin();
       iter != outputs_.end(); ++iter) {
    scoped_ptr<ShellAudioBus> audio_bus = (*iter)->PassAudioBusFromSource(
        static_cast<int32>(output_audio_bus->frames()),
        output_audio_bus->sample_type());

    if (audio_bus) {
      if (*silence && audio_bus->channels() == output_audio_bus->channels()) {
        output_audio_bus->Assign(*audio_bus);
      } else {
        MixAudioBuffer(owner_node_->channel_interpretation(), audio_bus.get(),
                       output_audio_bus);
      }
      *silence = false;
    }
  }
}

}  // namespace audio
}  // namespace cobalt
