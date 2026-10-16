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

// Implements the `sb_surface` and `sb_player` imports for one execution
// context on top of Starboard's SbPlayer.
//
// Each PlayerHost owns a display-critical player thread that makes every
// SbPlayer call, so the JS thread never blocks on Starboard. Starboard
// callbacks update internal state and queue `sb_event` records on the
// context's EventQueue.

#ifndef COBALT_STARBOARD_WASM_PLAYER_PLAYER_HOST_H_
#define COBALT_STARBOARD_WASM_PLAYER_PLAYER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/starboard_wasm/core/event_queue.h"
#include "cobalt/starboard_wasm/core/module.h"
#include "cobalt/starboard_wasm/player/player_abi.h"
#include "cobalt/starboard_wasm/player/surface_registry.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace cobalt {
namespace starboard_wasm {

class PlayerHost : public SurfaceRegistryObserver {
 public:
  static constexpr int64_t kInfoRefreshIntervalUs = 16'000;
  static constexpr size_t kMaxCachedCaps = 64;
  // StarboardRenderer's default for Media.MaxSamplesPerWrite.
  static constexpr int kDefaultMaxSamplesPerWrite = 12;

  explicit PlayerHost(const ModuleContext& context);
  PlayerHost(const PlayerHost&) = delete;
  PlayerHost& operator=(const PlayerHost&) = delete;
  ~PlayerHost() override;

  uint32_t SurfaceCreate(uint32_t flags);
  void SurfaceDestroy(uint32_t surface);
  void SurfaceSetBounds(uint32_t surface,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        int32_t z_index);
  // Returns the token that binds |surface| to a `<video>`, or "" if unknown.
  std::string SurfaceToken(uint32_t surface) const;

  uint32_t PlayerCreate(const uint8_t* audio,
                        uint32_t audio_len,
                        const uint8_t* video,
                        uint32_t video_len,
                        uint32_t surface,
                        uint32_t flags);
  void PlayerDestroy(uint32_t player);
  void PlayerSeek(uint32_t player, int64_t position_us, uint32_t ticket);
  uint32_t PlayerWriteSamples(uint32_t player,
                              uint32_t track,
                              const uint8_t* bytes,
                              uint32_t bytes_len,
                              const uint8_t* infos,
                              uint32_t count,
                              const uint8_t* config,
                              uint32_t config_len);
  void PlayerWriteEos(uint32_t player, uint32_t track);
  void PlayerSetRate(uint32_t player, int32_t rate_milli);
  void PlayerSetVolume(uint32_t player, int32_t volume_milli);
  uint32_t PlayerGetInfo(uint32_t player, uint8_t* out, uint32_t out_len);
  uint32_t MediaCaps(const char* mime,
                     uint32_t mime_len,
                     uint8_t* out,
                     uint32_t out_len);

  void OnSurfaceBoundsChanged(const std::string& token) override;

 private:
  struct StoredAudioConfig {
    me_audio_config raw{};
    std::string mime;
    std::vector<uint8_t> csd;
    SbMediaAudioStreamInfo stream_info{};
  };

  struct StoredVideoConfig {
    me_video_config raw{};
    std::string mime;
    std::string max_video_capabilities;
    SbMediaVideoStreamInfo stream_info{};
  };

  struct CallbackContext {
    PlayerHost* host;
    uint32_t id;
  };

  struct Surface {
    std::string token;
    bool has_guest_bounds = false;
    PlaneRect guest_bounds;
    int32_t z_index = 0;
  };

  // The MIME-dependent fields of me_media_caps.
  struct ProbedCaps {
    uint32_t can_play = 0;
    uint32_t max_width = 0;
    uint32_t max_height = 0;
    uint32_t max_frame_rate_milli = 0;
  };

  struct DeferredEvent {
    uint32_t type;
    int64_t a;
    int64_t b;
    bool coalesce;
  };

  struct PlayerEntry {
    uint32_t id = 0;
    uint32_t surface = 0;
    SbPlayer sb_player = kSbPlayerInvalid;
    bool create_failed = false;
    // Starboard calls back from its own threads as soon as SbPlayerCreate()
    // starts the player; until `sb_player` and the limits below are stored,
    // the player's events wait in `deferred_events` so the guest can't act
    // on a half-set-up player.
    bool published = false;
    std::vector<DeferredEvent> deferred_events;
    std::unique_ptr<CallbackContext> callback_context;
    uint32_t state = ME_PLAYER_NONE;
    // Starboard seek ticket, owned by the host; `guest_ticket` is the ticket
    // the guest passed to the latest seek and is what events report.
    int ticket = SB_PLAYER_INITIAL_TICKET;
    uint32_t guest_ticket = 0;
    bool destroying = false;
    bool destroyed_event_sent = false;
    bool has_applied_bounds = false;
    PlaneRect applied_bounds{};
    int32_t applied_z_index = 0;
    int64_t seek_time_us = 0;
    int64_t last_media_time_us = 0;
    int64_t last_media_time_retrieved_us = 0;
    int64_t last_audio_pts_us = -1;
    int64_t last_video_pts_us = -1;
    int32_t rate_milli = 1000;
    // A new SbPlayer plays at rate 1.0 and volume 1.0.
    int32_t last_sent_rate_milli = 1000;
    int32_t last_sent_volume_milli = 1000;
    bool audio_eos_written = false;
    bool video_eos_written = false;
    bool audio_needs_data_deferred = false;
    bool video_needs_data_deferred = false;
    int max_samples_per_write[ME_TRACK_COUNT] = {kDefaultMaxSamplesPerWrite,
                                                 kDefaultMaxSamplesPerWrite};
    int64_t audio_write_duration_us = kSbPlayerWriteDurationLocal;
    SbPlayerInfo info{};
    bool info_valid = false;
    int64_t info_us = 0;
    std::deque<StoredAudioConfig> audio_configs;
    std::deque<StoredVideoConfig> video_configs;
    std::unordered_set<const void*> allocated_buffers;
  };

  static void PopulateAudioConfigInPlace(const me_audio_config& cfg,
                                         StoredAudioConfig* out);
  static void PopulateVideoConfigInPlace(const me_video_config& cfg,
                                         StoredVideoConfig* out);
  static int64_t MediaTimeAge(const PlayerEntry& entry);
  // Takes up to a few dozen SbMediaCanPlayMimeAndKeySystem() calls.
  static ProbedCaps ProbeCaps(const std::string& mime, bool is_video);

  void DestroyAllOnPlayerThread();
  void CreateOnPlayerThread(uint32_t id);
  void UpdateAudioWriteDurationOnPlayerThread(uint32_t id);
  // Any thread; |mu_| held.
  void ReportCreateFailedLocked(PlayerEntry& entry);
  void QueuePlayerEventLocked(PlayerEntry& entry,
                              uint32_t type,
                              int64_t a,
                              int64_t b,
                              bool coalesce);
  void DestroyOnPlayerThread(uint32_t id);
  void SeekOnPlayerThread(uint32_t id, int64_t position_us, int ticket);
  void WriteOnPlayerThread(uint32_t id,
                           SbMediaType type,
                           std::vector<SbPlayerSampleInfo> samples);
  void WriteEosOnPlayerThread(uint32_t id, SbMediaType type);
  void SetRateOnPlayerThread(uint32_t id, int32_t rate_milli);
  void SetVolumeOnPlayerThread(uint32_t id, int32_t volume_milli);
  void ApplyBoundsOnPlayerThread();
  void RequestRefresh();
  void ScheduleRefreshTimer();
  void OnRefreshTimer();
  void RefreshOnPlayerThread();
  void ScheduleSurfaceProbe();
  void ProbeSurface();

  static void DeallocateSampleCB(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer);
  static void DecoderStatusCB(SbPlayer player,
                              void* context,
                              SbMediaType type,
                              SbPlayerDecoderState state,
                              int ticket);
  static void PlayerStatusCB(SbPlayer player,
                             void* context,
                             SbPlayerState state,
                             int ticket);
  static void PlayerErrorCB(SbPlayer player,
                            void* context,
                            SbPlayerError error,
                            const char* message);

  const scoped_refptr<base::SequencedTaskRunner> owner_;
  const raw_ref<EventQueue> events_;

  base::Thread player_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> player_runner_;

  mutable base::Lock mu_;
  uint32_t next_id_ = 1;
  std::unordered_map<uint32_t, PlayerEntry> players_;
  uint32_t next_surface_id_ = 1;
  std::unordered_map<uint32_t, Surface> surfaces_;
  // Where unpositioned surfaces go, from the latest PlayerSettings; empty if
  // unknown.
  PlaneRect viewport_;
  // Media.MaxSamplesPerWrite from the latest PlayerSettings.
  int max_samples_per_write_ = kDefaultMaxSamplesPerWrite;

  std::atomic<bool> refresh_requested_{false};
  std::atomic<bool> refresh_timer_armed_{false};

  // MediaCaps() results by MIME; used on |owner_| only.
  std::unordered_map<std::string, ProbedCaps> caps_cache_;

  bool refresh_timer_running_ = false;

  bool surface_probe_pending_ = false;
  std::atomic<int> pending_destroys_{0};
  std::atomic<bool> surface_waiter_{false};

  base::WeakPtr<PlayerHost> weak_this_;
  base::WeakPtrFactory<PlayerHost> weak_factory_{this};
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_PLAYER_HOST_H_
