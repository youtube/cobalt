// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/mojo/common/starboard/mojo_renderer_bypass_bridge.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"

namespace media {

MojoRendererBypassBridge::MojoRendererBypassBridge(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    TimeUpdateCB time_update_cb,
    StatisticsUpdateCB statistics_update_cb)
    : client_task_runner_(std::move(client_task_runner)),
      time_update_cb_(std::move(time_update_cb)),
      statistics_update_cb_(std::move(statistics_update_cb)),
      cond_var_(&lock_) {}

MojoRendererBypassBridge::~MojoRendererBypassBridge() {
  Invalidate();
}

void MojoRendererBypassBridge::Invalidate() {
  base::AutoLock auto_lock(lock_);
  active_ = false;
  audio_stream_ = nullptr;
  video_stream_ = nullptr;
  while (in_flight_reads_ > 0) {
    cond_var_.Wait();
  }
}

void MojoRendererBypassBridge::SetStreams(DemuxerStream* audio,
                                          DemuxerStream* video) {
  base::AutoLock auto_lock(lock_);
  audio_stream_ = audio;
  video_stream_ = video;
}

void MojoRendererBypassBridge::PostTimeUpdate(base::TimeDelta time,
                                              base::TimeDelta max_time,
                                              base::TimeTicks capture_time) {
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoRendererBypassBridge::RunTimeUpdateOnClientThread,
                     this, time, max_time, capture_time));
}

void MojoRendererBypassBridge::PostStatisticsUpdate(
    const PipelineStatistics& stats) {
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MojoRendererBypassBridge::RunStatisticsUpdateOnClientThread, this,
          stats));
}

bool MojoRendererBypassBridge::Read(DemuxerStream::Type type,
                                    uint32_t count,
                                    DemuxerStream::ReadCB read_cb) {
  DemuxerStream* stream = nullptr;
  {
    base::AutoLock auto_lock(lock_);
    if (!active_) {
      std::move(read_cb).Run(DemuxerStream::kAborted, {});
      return false;
    }
    stream = (type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_);
    if (stream) {
      in_flight_reads_++;
    }
  }
  if (!stream) {
    std::move(read_cb).Run(DemuxerStream::kAborted, {});
    return false;
  }

  // Wrap the callback to decrement in_flight_reads_ when it runs.
  base::ScopedClosureRunner scoped_decrement(
      base::BindOnce(&MojoRendererBypassBridge::DecrementInFlightReads, this));

  auto wrapped_cb =
      base::BindOnce(&MojoRendererBypassBridge::OnReadDone, this,
                     std::move(read_cb), std::move(scoped_decrement));

  stream->Read(count, std::move(wrapped_cb));
  return true;
}

bool MojoRendererBypassBridge::IsActive() const {
  base::AutoLock auto_lock(lock_);
  return active_;
}

AudioDecoderConfig MojoRendererBypassBridge::GetAudioConfig() const {
  base::AutoLock auto_lock(lock_);
  if (!active_ || !audio_stream_) {
    return AudioDecoderConfig();
  }
  return audio_stream_->audio_decoder_config();
}

VideoDecoderConfig MojoRendererBypassBridge::GetVideoConfig() const {
  base::AutoLock auto_lock(lock_);
  if (!active_ || !video_stream_) {
    return VideoDecoderConfig();
  }
  return video_stream_->video_decoder_config();
}

bool MojoRendererBypassBridge::SupportsConfigChanges(
    DemuxerStream::Type type) const {
  base::AutoLock auto_lock(lock_);
  if (!active_) {
    return false;
  }
  DemuxerStream* stream =
      (type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_);
  return stream ? stream->SupportsConfigChanges() : false;
}

StreamLiveness MojoRendererBypassBridge::GetLiveness(
    DemuxerStream::Type type) const {
  base::AutoLock auto_lock(lock_);
  if (!active_) {
    return StreamLiveness::kUnknown;
  }
  DemuxerStream* stream =
      (type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_);
  return stream ? stream->liveness() : StreamLiveness::kUnknown;
}

void MojoRendererBypassBridge::RunTimeUpdateOnClientThread(
    base::TimeDelta time,
    base::TimeDelta max_time,
    base::TimeTicks capture_time) {
  time_update_cb_.Run(time, max_time, capture_time);
}

void MojoRendererBypassBridge::RunStatisticsUpdateOnClientThread(
    const PipelineStatistics& stats) {
  statistics_update_cb_.Run(stats);
}

void MojoRendererBypassBridge::OnReadDone(
    DemuxerStream::ReadCB read_cb,
    base::ScopedClosureRunner scoped_decrement,
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  std::move(read_cb).Run(status, std::move(buffers));
}

void MojoRendererBypassBridge::DecrementInFlightReads() {
  base::AutoLock auto_lock(lock_);
  in_flight_reads_--;
  if (in_flight_reads_ == 0) {
    cond_var_.Signal();
  }
}

// static
void BypassBridgeRegistry::Register(
    uint32_t id,
    scoped_refptr<MojoRendererBypassBridge> bridge) {
  base::AutoLock auto_lock(GetLock());
  GetMap()[id] = bridge;
}

// static
void BypassBridgeRegistry::Unregister(uint32_t id) {
  base::AutoLock auto_lock(GetLock());
  GetMap().erase(id);
}

// static
scoped_refptr<MojoRendererBypassBridge> BypassBridgeRegistry::Get(uint32_t id) {
  base::AutoLock auto_lock(GetLock());
  auto it = GetMap().find(id);
  return it != GetMap().end() ? it->second : nullptr;
}

// static
base::Lock& BypassBridgeRegistry::GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// static
std::map<uint32_t, scoped_refptr<MojoRendererBypassBridge>>&
BypassBridgeRegistry::GetMap() {
  static base::NoDestructor<
      std::map<uint32_t, scoped_refptr<MojoRendererBypassBridge>>>
      id_to_bridge_map;
  return *id_to_bridge_map;
}

}  // namespace media
