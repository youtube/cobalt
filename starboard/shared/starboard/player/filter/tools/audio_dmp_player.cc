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

#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/directory.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/system.h"

namespace {

using starboard::shared::starboard::player::video_dmp::VideoDmpReader;
using starboard::shared::starboard::player::filter::AudioRenderer;
using starboard::shared::starboard::player::filter::PlayerComponents;
using starboard::shared::starboard::player::JobThread;
using starboard::scoped_ptr;

// TODO: Merge test file resolving function with the ones used in the player
// filter tests.
std::string GetTestInputDirectory() {
  const size_t kPathSize = SB_FILE_MAX_PATH + 1;

  char content_path[kPathSize];
  SB_CHECK(
      SbSystemGetPath(kSbSystemPathContentDirectory, content_path, kPathSize));
  std::string directory_path =
      std::string(content_path) + SB_FILE_SEP_CHAR + "test" + SB_FILE_SEP_CHAR +
      "starboard" + SB_FILE_SEP_CHAR + "shared" + SB_FILE_SEP_CHAR +
      "starboard" + SB_FILE_SEP_CHAR + "player" + SB_FILE_SEP_CHAR + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path;
}

std::string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + SB_FILE_SEP_CHAR + filename;
}

scoped_ptr<VideoDmpReader> s_video_dmp_reader;
scoped_ptr<AudioRenderer> s_audio_renderer;
int s_audio_sample_index;
scoped_ptr<JobThread> s_job_thread;
SbTime s_duration;

void OnTimer() {
  if (!s_audio_renderer->CanAcceptMoreData()) {
    s_job_thread->job_queue()->Schedule(std::bind(OnTimer), kSbTimeMillisecond);
    return;
  }

  if (s_audio_sample_index == s_video_dmp_reader->number_of_audio_buffers()) {
    SB_LOG(INFO) << "EOS written, duration " << s_duration << " microseconds.";
    s_audio_renderer->WriteEndOfStream();
    return;
  } else {
    auto input_buffer =
        s_video_dmp_reader->GetAudioInputBuffer(s_audio_sample_index);
    s_duration = input_buffer->timestamp();
    s_audio_renderer->WriteSample(input_buffer);
    ++s_audio_sample_index;
  }

  s_job_thread->job_queue()->Schedule(std::bind(OnTimer));
}

void ErrorCB(SbPlayerError error, const std::string& error_message) {
  SB_NOTREACHED() << "ErrorCB should never be called by audio_dmp_player";
}

void PrerolledCB() {
  SB_LOG(INFO) << "Playback started.";
  s_audio_renderer->Play();
}

void EndedCB() {
  SB_LOG(INFO) << "Playback finished.";
  s_audio_renderer.reset();
  s_video_dmp_reader.reset();
  SbSystemRequestStop(0);
}

void Start(const char* filename) {
  SB_LOG(INFO) << "Loading " << filename;
  s_video_dmp_reader.reset(
      new VideoDmpReader(ResolveTestFileName(filename).c_str()));
  scoped_ptr<PlayerComponents> player_components = PlayerComponents::Create();
  PlayerComponents::AudioParameters audio_parameters = {
      s_video_dmp_reader->audio_codec(), s_video_dmp_reader->audio_header(),
      kSbDrmSystemInvalid};
  s_audio_renderer = player_components->CreateAudioRenderer(audio_parameters);
  SB_DCHECK(s_audio_renderer);

  using std::placeholders::_1;
  using std::placeholders::_2;

  s_audio_renderer->Initialize(std::bind(ErrorCB, _1, _2),
                               std::bind(PrerolledCB), std::bind(EndedCB));
  s_audio_renderer->SetPlaybackRate(1.0);
  s_audio_renderer->SetVolume(1.0);
  s_audio_renderer->Seek(0);
  s_job_thread->job_queue()->Schedule(std::bind(OnTimer));
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      SB_DCHECK(data);

      if (data->argument_count < 2) {
        SB_LOG(INFO) << "Usage: audio_dmp_player <dmp file name>";
        SB_LOG(INFO) << "e.g. audio_dmp_player beneath_the_canopy_avc_aac.dmp";
        SB_LOG(INFO) << "     audio_dmp_player beneath_the_canopy_vp9_opus.dmp";
        SbSystemRequestStop(0);
        return;
      }

      s_job_thread.reset(new JobThread("audio"));
      s_job_thread->job_queue()->Schedule(
          std::bind(Start, data->argument_values[1]));
      break;
    }
    case kSbEventTypeStop: {
      s_job_thread.reset();
      break;
    }
    default:
      break;
  }
}
