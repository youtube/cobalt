/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/audio/null_audio_streamer.h"

#include <algorithm>

#include "media/audio/audio_parameters.h"
#include "media/mp4/aac.h"

namespace media {

NullAudioStreamer::NullAudioStreamer()
    : null_streamer_thread_("Null Audio Streamer") {
  null_streamer_thread_.Start();
  null_streamer_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&NullAudioStreamer::StartNullStreamer,
                            base::Unretained(this)));
}

NullAudioStreamer::~NullAudioStreamer() {
  null_streamer_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&NullAudioStreamer::StopNullStreamer, base::Unretained(this)));
  null_streamer_thread_.Stop();
}

ShellAudioStreamer::Config NullAudioStreamer::GetConfig() const {
  // Reasonable looking settings.
  const uint32 initial_rebuffering_frames_per_channel =
      mp4::AAC::kSamplesPerFrame * 32;
  const uint32 sink_buffer_size_in_frames_per_channel =
      initial_rebuffering_frames_per_channel * 8;
  const uint32 max_hardware_channels = 2;

  return Config(Config::INTERLEAVED, initial_rebuffering_frames_per_channel,
                sink_buffer_size_in_frames_per_channel, max_hardware_channels,
                sizeof(float) /* bytes_per_sample */);
}

bool NullAudioStreamer::AddStream(ShellAudioStream* stream) {
  base::AutoLock auto_lock(streams_lock_);
  streams_.insert(std::make_pair(stream, NullAudioStream()));
  return true;
}

void NullAudioStreamer::RemoveStream(ShellAudioStream* stream) {
  base::AutoLock auto_lock(streams_lock_);
  DLOG(INFO) << "Remove";
  streams_.erase(stream);
}

bool NullAudioStreamer::HasStream(ShellAudioStream* stream) const {
  base::AutoLock auto_lock(streams_lock_);
  return streams_.find(stream) != streams_.end();
}

void NullAudioStreamer::StartNullStreamer() {
  last_run_time_ = base::Time::Now();
  advance_streams_timer_.emplace();
  advance_streams_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(10),
      base::Bind(&NullAudioStreamer::AdvanceStreams, base::Unretained(this)));
}

void NullAudioStreamer::StopNullStreamer() {
  advance_streams_timer_->Stop();
  advance_streams_timer_ = base::nullopt;
}

void NullAudioStreamer::AdvanceStreams() {
  base::Time now = base::Time::Now();
  base::TimeDelta time_played = now - last_run_time_;
  last_run_time_ = now;

  base::AutoLock auto_lock(streams_lock_);
  for (NullAudioStreamMap::iterator it = streams_.begin(); it != streams_.end();
       ++it) {
    PullFrames(it->first, time_played, &it->second);
  }
}

void NullAudioStreamer::PullFrames(ShellAudioStream* stream,
                                   base::TimeDelta time_played,
                                   NullAudioStream* null_stream) {
  // Calculate how many frames were consumed.
  int sample_rate = stream->GetAudioParameters().sample_rate();
  uint32 frames_played = sample_rate * time_played.InSecondsF();
  frames_played = std::min(frames_played, null_stream->total_available_frames);
  if (!stream->PauseRequested()) {
    stream->ConsumeFrames(frames_played);
  }
  // Pull more frames.
  stream->PullFrames(NULL, &null_stream->total_available_frames);
}

}  // namespace media
