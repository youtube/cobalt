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

#ifndef COBALT_STARBOARD_WASM_WASM_PLAYER_HOST_H_
#define COBALT_STARBOARD_WASM_WASM_PLAYER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/starboard_wasm/wasm_starboard_abi.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/extension/player_settings.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
class SbPlayerInterface;
}  // namespace media

namespace cobalt {
namespace starboard_wasm {

class EventQueue;
class GuestMemory;

// Implements H5vccWasmPlayer and H5vccWasmSurface for one execution context:
// each player operation is the SbPlayer call of the same name, made on the
// calling sequence, which must be `events`' sequence. The host keeps only
// what a native caller of SbPlayer would: player handles and sample copies
// until Starboard deallocates them. It rejects only inputs that would crash
// the process (see wasm_starboard_abi.h). Starboard callbacks push to `events`
// on their own threads.
//
// A surface is an overlay plane id. The compositor reports where the hole of
// the <video> showing that plane is, and the host positions the surface's
// players there.
class WasmPlayerHost : public cobalt::media::mojom::VideoGeometryChangeClient {
 public:
  using VideoGeometrySubscriberBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<
          cobalt::media::mojom::VideoGeometryChangeSubscriber>)>;

  // `sbplayer` must outlive the SbPlayerDestroy() calls that the host posts to
  // the thread pool.
  WasmPlayerHost(scoped_refptr<EventQueue> events,
                 ::media::SbPlayerInterface& sbplayer);
  WasmPlayerHost(const WasmPlayerHost&) = delete;
  WasmPlayerHost& operator=(const WasmPlayerHost&) = delete;
  ~WasmPlayerHost() override;

  // The ExperimentalFeaturesConfiguration extension, for the calling thread.
  static bool SetExperimentalFeaturesForCurrentThread(GuestMemory& memory,
                                                      uint32_t entries,
                                                      uint32_t count);
  static void SetExperimentalFeaturesApiForTesting(
      const StarboardExtensionExperimentalFeaturesConfigurationApi* api);

  // The PlayerSettings extension, for the calling thread. False if the
  // platform lacks the function. An empty resolution clears it.
  static bool SetMaxVideoInputSizeForCurrentThread(
      int32_t max_video_input_size);
  static bool SetMaxVideoResolutionForCurrentThread(
      GuestMemory& memory,
      uint32_t max_video_resolution,
      uint32_t max_video_resolution_size);
  static void SetPlayerSettingsApiForTesting(
      const StarboardExtensionPlayerSettingsApi* api);

  // Prevents a document's per-thread features and settings from reaching
  // players that the thread's next document creates.
  static void ResetForCurrentThread();

  static ::media::SbPlayerInterface& DefaultSbPlayerInterface();

  // Connects hosts to the compositor's VideoGeometrySetterService; called on
  // any thread. Surfaces created while there is no binder don't position
  // their players.
  static void SetVideoGeometrySubscriberBinder(
      VideoGeometrySubscriberBinder binder);

  uint32_t CreateSurface();
  void DestroySurface(uint32_t surface);
  std::string SurfaceToken(uint32_t surface) const;

  uint32_t Create(GuestMemory& memory,
                  uint32_t surface,
                  uint32_t creation_param,
                  uint32_t creation_param_size);
  void Destroy(uint32_t player);
  bool Seek(uint32_t player, double seek_to_timestamp, int32_t ticket);
  bool WriteSamples(GuestMemory& memory,
                    uint32_t player,
                    int32_t sample_type,
                    uint32_t sample_infos,
                    uint32_t count);
  bool WriteEndOfStream(uint32_t player, int32_t stream_type);
  void SetBounds(uint32_t player,
                 int32_t z_index,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height);
  bool SetPlaybackRate(uint32_t player, double playback_rate);
  void SetVolume(uint32_t player, double volume);
  bool GetInfo(GuestMemory& memory, uint32_t player, uint32_t info);
  bool GetAudioConfiguration(GuestMemory& memory,
                             uint32_t player,
                             int32_t index,
                             uint32_t out_audio_configuration);
  // Copies up to `out_size` bytes of the player's last error message to `out`
  // and returns its full length; 0 if there is none or `out` is out of bounds.
  uint32_t GetErrorMessage(GuestMemory& memory,
                           uint32_t player,
                           uint32_t out,
                           uint32_t out_size);
  int32_t GetMaximumNumberOfSamplesPerWrite(uint32_t player,
                                            int32_t sample_type);
  int32_t GetPreferredOutputMode(GuestMemory& memory,
                                 uint32_t creation_param,
                                 uint32_t creation_param_size);
  int32_t CanPlayMimeAndKeySystem(GuestMemory& memory,
                                  uint32_t mime,
                                  uint32_t mime_size,
                                  uint32_t key_system,
                                  uint32_t key_system_size);
  bool CanChangeType(GuestMemory& memory,
                     uint32_t current_mime,
                     uint32_t current_mime_size,
                     uint32_t new_mime,
                     uint32_t new_mime_size);

  size_t SampleCopiesForTesting(uint32_t player);

  // cobalt::media::mojom::VideoGeometryChangeClient:
  void OnVideoGeometryChange(const gfx::RectF& rect,
                             gfx::OverlayTransform transform) override;

 private:
  struct Player;
  struct CallbackContext;
  struct Surface {
    base::UnguessableToken token;
    mojo::ReceiverId receiver = 0;
    std::optional<gfx::Rect> rect;
  };

  Player* Find(uint32_t player);
  static bool NeedsData(Player& player, SbMediaType type);
  static void ClearNeedsData(Player& player, SbMediaType type);
  void ApplyBounds(Player& player);
  void SetSurfaceVideoSize(const Player& player,
                           const SbMediaVideoStreamInfo& info) const;

  void OnDeallocateSample(uint32_t player, const void* sample_buffer);

  static void PushEvent(void* context, WasmSbEvent event);

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

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<EventQueue> events_;
  const raw_ref<::media::SbPlayerInterface> sbplayer_;
  const scoped_refptr<base::SequencedTaskRunner> destroy_task_runner_;
  uint32_t next_player_ = 1;
  std::map<uint32_t, std::unique_ptr<Player>> players_;
  uint32_t next_surface_ = 1;
  std::map<uint32_t, Surface> surfaces_;
  mojo::Remote<cobalt::media::mojom::VideoGeometryChangeSubscriber>
      geometry_subscriber_;
  // Keyed by surface. The service keeps one client per plane and doesn't
  // replay geometry, so the host subscribes once per surface and keeps the
  // last rect for players created later.
  mojo::ReceiverSet<cobalt::media::mojom::VideoGeometryChangeClient, uint32_t>
      geometry_receivers_;
  // Copied from other threads, which must not call GetWeakPtr().
  base::WeakPtr<WasmPlayerHost> weak_this_;
  base::WeakPtrFactory<WasmPlayerHost> weak_factory_{this};
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_WASM_PLAYER_HOST_H_
