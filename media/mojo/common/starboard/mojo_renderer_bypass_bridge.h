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

#ifndef MEDIA_MOJO_COMMON_STARBOARD_MOJO_RENDERER_BYPASS_BRIDGE_H_
#define MEDIA_MOJO_COMMON_STARBOARD_MOJO_RENDERER_BYPASS_BRIDGE_H_

#include <map>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"

namespace media {

// MojoRendererBypassBridge facilitates high-performance media playback in
// single-process mode by bypassing Mojo IPC for media data transfers and
// high-frequency callbacks (e.g., time and statistics updates).
//
// Lifetime and Ownership:
// - It is a thread-safe ref-counted object.
// - It is co-owned by MojoRenderer (on the client side) and MojoRendererService
//   (on the service side) via scoped_refptr.
// - The client registers the bridge in the BypassBridgeRegistry during
//   initialization and unregisters it on destruction.
// - On destruction, the client calls Invalidate() to signal the service that
//   the client is dead. The bridge remains alive until both client and service
//   release their references.
//
// Threading Model:
// - Thread-safe. All public methods accessing the shared state (audio/video
//   streams, configurations, active status) are guarded by `lock_`.
// - Media data reads (Read()) are initiated outside the lock to prevent
//   deadlocks if the demuxer calls the read callback synchronously. An internal
//   in-flight counter and condition variable ensure that the streams are not
//   destroyed while a Read() is in progress.
// - Callbacks to the client (PostTimeUpdate(), PostStatisticsUpdate()) are
//   marshalled to the client's task runner (typically the Media thread) and
//   executed safely using a WeakPtr to the MojoRenderer.
class MojoRendererBypassBridge
    : public base::RefCountedThreadSafe<MojoRendererBypassBridge> {
 public:
  using TimeUpdateCB = base::RepeatingCallback<
      void(base::TimeDelta, base::TimeDelta, base::TimeTicks)>;
  using StatisticsUpdateCB =
      base::RepeatingCallback<void(const PipelineStatistics&)>;

  MojoRendererBypassBridge(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      TimeUpdateCB time_update_cb,
      StatisticsUpdateCB statistics_update_cb);

  MojoRendererBypassBridge(const MojoRendererBypassBridge&) = delete;
  MojoRendererBypassBridge& operator=(const MojoRendererBypassBridge&) = delete;

  void Invalidate();
  void SetStreams(DemuxerStream* audio, DemuxerStream* video);

  void PostTimeUpdate(base::TimeDelta time,
                      base::TimeDelta max_time,
                      base::TimeTicks capture_time);
  void PostStatisticsUpdate(const PipelineStatistics& stats);

  bool Read(DemuxerStream::Type type,
            uint32_t count,
            DemuxerStream::ReadCB read_cb);
  bool IsActive() const;

  AudioDecoderConfig GetAudioConfig() const;
  VideoDecoderConfig GetVideoConfig() const;
  bool SupportsConfigChanges(DemuxerStream::Type type) const;
  StreamLiveness GetLiveness(DemuxerStream::Type type) const;

 private:
  friend class base::RefCountedThreadSafe<MojoRendererBypassBridge>;
  ~MojoRendererBypassBridge();

  void RunTimeUpdateOnClientThread(base::TimeDelta time,
                                   base::TimeDelta max_time,
                                   base::TimeTicks capture_time);
  void RunStatisticsUpdateOnClientThread(const PipelineStatistics& stats);
  void OnReadDone(DemuxerStream::ReadCB read_cb,
                  base::ScopedClosureRunner scoped_decrement,
                  DemuxerStream::Status status,
                  DemuxerStream::DecoderBufferVector buffers);
  void DecrementInFlightReads();

  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  const TimeUpdateCB time_update_cb_;
  const StatisticsUpdateCB statistics_update_cb_;

  mutable base::Lock lock_;
  base::ConditionVariable cond_var_;
  int in_flight_reads_ GUARDED_BY(lock_) = 0;
  bool active_ GUARDED_BY(lock_) = true;
  raw_ptr<DemuxerStream> audio_stream_ GUARDED_BY(lock_) = nullptr;
  raw_ptr<DemuxerStream> video_stream_ GUARDED_BY(lock_) = nullptr;
};

// BypassBridgeRegistry is a global, thread-safe registry used to share
// MojoRendererBypassBridge instances between the client (MojoRenderer) and
// the service (MojoRendererService) in single-process mode.
//
// Since Mojo IPC is bypassed, we cannot pass ref-counted pointers directly
// through Mojo interfaces without risking memory leaks if the Mojo connection
// is prematurely severed. Instead:
// 1. The client generates a unique ID, associates it with the bridge, and
//    registers it here.
// 2. The client passes only the ID (uint32_t) over Mojo to the service.
//    (If the Mojo connection fails before the service receives the ID, the
//    client destructor will safely unregister and release the bridge).
// 3. The service retrieves the bridge from this registry using the ID.
// 4. The client unregisters the ID upon its destruction.
//
// Threading Model:
// - Thread-safe. All static methods (Register, Unregister, Get) are guarded
//   by an internal static lock.
class BypassBridgeRegistry {
 public:
  static void Register(uint32_t id,
                       scoped_refptr<MojoRendererBypassBridge> bridge);
  static void Unregister(uint32_t id);
  static scoped_refptr<MojoRendererBypassBridge> Get(uint32_t id);

 private:
  static base::Lock& GetLock();
  static std::map<uint32_t, scoped_refptr<MojoRendererBypassBridge>>& GetMap();
};

}  // namespace media
#endif  // MEDIA_MOJO_COMMON_STARBOARD_MOJO_RENDERER_BYPASS_BRIDGE_H_
