// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/event.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/system.h"

namespace {

using ::starboard::PlayerComponents;

std::unique_ptr<VideoDmpReader> s_video_dmp_reader;
std::unique_ptr<PlayerComponents> s_player_components;
int s_audio_sample_index;
std::unique_ptr<JobThread> s_job_thread;
int64_t s_duration;

static void DeallocateSampleFunc(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {}

starboard::scoped_refptr<InputBuffer> GetAudioInputBuffer(size_t index) {
  auto player_sample_info =
      s_video_dmp_reader->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
  return new InputBuffer(DeallocateSampleFunc, NULL, NULL, player_sample_info);
}

void OnTimer() {
  if (!s_player_components->GetAudioRenderer()->CanAcceptMoreData()) {
    s_job_thread->Schedule(OnTimer, 1000);
    return;
  }

  if (s_audio_sample_index == s_video_dmp_reader->number_of_audio_buffers()) {
    SB_LOG(INFO) << "EOS written, duration " << s_duration << " microseconds.";
    s_player_components->GetAudioRenderer()->WriteEndOfStream();
    return;
  } else {
    InputBuffers input_buffers;
    auto input_buffer = GetAudioInputBuffer(s_audio_sample_index);

    s_duration = input_buffer->timestamp();
    input_buffers.push_back(std::move(input_buffer));
    s_player_components->GetAudioRenderer()->WriteSamples(input_buffers);
    ++s_audio_sample_index;
  }

  s_job_thread->Schedule(OnTimer);
}

void ErrorCB(SbPlayerError error, const std::string& error_message) {
  SB_NOTREACHED() << "ErrorCB is called with error " << error << ", "
                  << error_message;
}

void PrerolledCB() {
  SB_LOG(INFO) << "Playback started.";
  s_player_components->GetMediaTimeProvider()->Play();
}

void EndedCB() {
  SB_LOG(INFO) << "Playback finished.";
  s_player_components.reset();
  s_video_dmp_reader.reset();
  SbSystemRequestStop(0);
}

void Start(const char* filename) {
  SB_LOG(INFO) << "Loading " << filename;
  s_video_dmp_reader.reset(new VideoDmpReader(filename));
  std::unique_ptr<PlayerComponents::Factory> factory =
      PlayerComponents::Factory::Create();
  PlayerComponents::Factory::CreationParameters creation_parameters(
      s_video_dmp_reader->audio_stream_info(), s_job_thread->job_queue());
  auto player_components_result =
      factory->CreateComponents(creation_parameters);
  if (player_components_result) {
    s_player_components = std::move(player_components_result.value());
  } else {
    SB_LOG(ERROR) << "Failed to create player components: "
                  << player_components_result.error();
    return;
  }
  SB_DCHECK(s_player_components);
  SB_DCHECK(s_player_components->GetAudioRenderer());

  using std::placeholders::_1;
  using std::placeholders::_2;

  s_player_components->GetAudioRenderer()->Initialize(
      std::bind(ErrorCB, _1, _2), std::bind(PrerolledCB), std::bind(EndedCB));
  s_player_components->GetMediaTimeProvider()->SetPlaybackRate(1.0);
  s_player_components->GetAudioRenderer()->SetVolume(1.0);
  s_player_components->GetMediaTimeProvider()->Seek(0);
  s_job_thread->Schedule(OnTimer);
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      SB_DCHECK(data);

      if (data->argument_count < 2) {
        SB_LOG(INFO) << "Usage: audio_dmp_player <dmp file name>";
        SB_LOG(INFO)
            << "e.g. audio_dmp_player beneath_the_canopy_aac_stereo.dmp";
        SB_LOG(INFO)
            << "     audio_dmp_player beneath_the_canopy_opus_stereo.dmp";
        SbSystemRequestStop(0);
        return;
      }

      s_job_thread = JobThread::Create("audio");
      s_job_thread->Schedule(
          // Capture filename by value, since |data| is only valid for the
          // lifetime of SbEventHandle.
          [filename = std::string(data->argument_values[1])] {
            Start(filename.c_str());
          });
      break;
    }
    case kSbEventTypeStop: {
      if (s_job_thread) {
        s_job_thread->Stop();
        s_job_thread.reset();
      }
      break;
    }
    default:
      break;
  }
}
