// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_AUDIO_AUDIO_BUFFER_SOURCE_NODE_H_
#define COBALT_AUDIO_AUDIO_BUFFER_SOURCE_NODE_H_

#include <memory>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "cobalt/audio/audio_buffer.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/base/tokens.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media/base/interleaved_sinc_resampler.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace audio {

// This represents an audio source from an in-memory audio asset in an
// AudioBuffer. It is useful for playing short audio assets which require a high
// degree of scheduling flexibility (can playback in rhythmically perfect ways).
//   https://www.w3.org/TR/webaudio/#AudioBufferSourceNode
class AudioBufferSourceNode : public AudioNode {
  typedef media::InterleavedSincResampler InterleavedSincResampler;
  typedef media::AudioBus AudioBus;

 public:
  AudioBufferSourceNode(script::EnvironmentSettings* settings,
                        AudioContext* context);

  // Web API: AudioBufferSourceNode
  //
  // Represents the audio asset to be played.
  const scoped_refptr<AudioBuffer>& buffer() const { return buffer_; }
  void set_buffer(const scoped_refptr<AudioBuffer>& buffer);

  // The Start method is used to schedule a sound to playback at an exact time.
  // The parameters are used to define the time to start playing based on the
  // context's timeline. (0 or a time less than the context's current time means
  // "now"). The parameters are in seconds.
  void Start(double when, double offset,
             script::ExceptionState* exception_state);
  void Start(double when, double offset, double duration,
             script::ExceptionState* exception_state);

  // The Stop method is used to schedule a sound to stop playback at an exact
  // time. |when| is the time to stop playing based on the context's timeline.
  // (0 or a time less than the context's current time means "now").
  // If it hasn't been set explicitly, then the sound will not stop playing
  // or will stop when the end of the AudioBuffer has been reached.
  // The parameter is in seconds.
  void Stop(double when, script::ExceptionState* exception_state);

  // A property used to set the EventHandler for the ended event that is
  // dispatched to AudioBufferSourceNode node types. When the playback of the
  // buffer for an AudioBufferSourceNode is finished, an event of type Event
  // will be dispatched to the event handler.
  const EventListenerScriptValue* onended() const {
    return GetAttributeEventListener(base::Tokens::ended());
  }
  void set_onended(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::ended(), event_listener);
  }

  std::unique_ptr<AudioBus> PassAudioBusFromSource(int32 number_of_frames,
                                                   SampleType sample_type,
                                                   bool* finished) override;

  DEFINE_WRAPPABLE_TYPE(AudioBufferSourceNode);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  ~AudioBufferSourceNode() override;

 private:
  void RemoveBufferSource();

  enum State {
    kNone,
    kStarted,
    kStopped,
  };

  scoped_refptr<AudioBuffer> buffer_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  State state_;

  // |read_index_| is a sample-frame index into out buffer representing the
  // current playback position.
  int32 read_index_;

  // |interleaved_resampler_| is the InterleavedSincResampler object that will
  // be used if resampling needs to occur.
  std::unique_ptr<InterleavedSincResampler> interleaved_resampler_;

  // |buffer_source_added_| indicates whether this AudioBufferSourceNode object
  // has been added to AudioContext's |buffer_sources_|
  bool buffer_source_added_;

  const float sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(AudioBufferSourceNode);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_BUFFER_SOURCE_NODE_H_
