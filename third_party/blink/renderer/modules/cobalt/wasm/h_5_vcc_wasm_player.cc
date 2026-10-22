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

#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_player.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "cobalt/starboard_wasm/event_queue.h"
#include "cobalt/starboard_wasm/guest_memory.h"
#include "cobalt/starboard_wasm/wasm_player_host.h"
#include "cobalt/starboard_wasm/wasm_starboard_abi.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_imports.h"

namespace blink {

using cobalt::starboard_wasm::GuestMemory;
using cobalt::starboard_wasm::WasmPlayerHost;
using cobalt::starboard_wasm::WasmSbEvent;

H5vccWasmPlayer::H5vccWasmPlayer(H5vccWasmImports& imports)
    : imports_(&imports) {}

bool H5vccWasmPlayer::setExperimentalFeaturesForCurrentThread(
    v8::Isolate* isolate,
    uint32_t entries,
    uint32_t count) {
  if (!imports_->host()) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(
      memory, entries, count);
}

bool H5vccWasmPlayer::setMaxVideoInputSizeForCurrentThread(
    int32_t max_video_input_size) {
  return imports_->host() &&
         WasmPlayerHost::SetMaxVideoInputSizeForCurrentThread(
             max_video_input_size);
}

bool H5vccWasmPlayer::setMaxVideoResolutionForCurrentThread(
    v8::Isolate* isolate,
    uint32_t max_video_resolution,
    uint32_t max_video_resolution_size) {
  if (!imports_->host()) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(
      memory, max_video_resolution, max_video_resolution_size);
}

uint32_t H5vccWasmPlayer::create(v8::Isolate* isolate,
                                 uint32_t surface,
                                 uint32_t creation_param,
                                 uint32_t creation_param_size) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return 0;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->Create(memory, surface, creation_param, creation_param_size);
}

void H5vccWasmPlayer::destroy(uint32_t player) {
  if (WasmPlayerHost* host = imports_->host()) {
    host->Destroy(player);
  }
}

bool H5vccWasmPlayer::seek(uint32_t player,
                           double seek_to_timestamp,
                           int32_t ticket) {
  WasmPlayerHost* host = imports_->host();
  return host && host->Seek(player, seek_to_timestamp, ticket);
}

bool H5vccWasmPlayer::writeSamples(v8::Isolate* isolate,
                                   uint32_t player,
                                   int32_t sample_type,
                                   uint32_t sample_infos,
                                   uint32_t count) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->WriteSamples(memory, player, sample_type, sample_infos, count);
}

bool H5vccWasmPlayer::writeEndOfStream(uint32_t player, int32_t stream_type) {
  WasmPlayerHost* host = imports_->host();
  return host && host->WriteEndOfStream(player, stream_type);
}

void H5vccWasmPlayer::setBounds(uint32_t player,
                                int32_t z_index,
                                int32_t x,
                                int32_t y,
                                int32_t width,
                                int32_t height) {
  if (WasmPlayerHost* host = imports_->host()) {
    host->SetBounds(player, z_index, x, y, width, height);
  }
}

bool H5vccWasmPlayer::setPlaybackRate(uint32_t player, double playback_rate) {
  WasmPlayerHost* host = imports_->host();
  return host && host->SetPlaybackRate(player, playback_rate);
}

void H5vccWasmPlayer::setVolume(uint32_t player, double volume) {
  if (WasmPlayerHost* host = imports_->host()) {
    host->SetVolume(player, volume);
  }
}

bool H5vccWasmPlayer::getInfo(v8::Isolate* isolate,
                              uint32_t player,
                              uint32_t info) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->GetInfo(memory, player, info);
}

bool H5vccWasmPlayer::getAudioConfiguration(v8::Isolate* isolate,
                                            uint32_t player,
                                            int32_t index,
                                            uint32_t out) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->GetAudioConfiguration(memory, player, index, out);
}

uint32_t H5vccWasmPlayer::getErrorMessage(v8::Isolate* isolate,
                                          uint32_t player,
                                          uint32_t out,
                                          uint32_t out_size) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return 0;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->GetErrorMessage(memory, player, out, out_size);
}

int32_t H5vccWasmPlayer::getMaximumNumberOfSamplesPerWrite(
    uint32_t player,
    int32_t sample_type) {
  WasmPlayerHost* host = imports_->host();
  return host ? host->GetMaximumNumberOfSamplesPerWrite(player, sample_type)
              : 0;
}

int32_t H5vccWasmPlayer::getPreferredOutputMode(v8::Isolate* isolate,
                                                uint32_t creation_param,
                                                uint32_t creation_param_size) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return kSbPlayerOutputModeInvalid;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->GetPreferredOutputMode(memory, creation_param,
                                      creation_param_size);
}

int32_t H5vccWasmPlayer::canPlayMimeAndKeySystem(v8::Isolate* isolate,
                                                 uint32_t mime,
                                                 uint32_t mime_size,
                                                 uint32_t key_system,
                                                 uint32_t key_system_size) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return kSbMediaSupportTypeNotSupported;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->CanPlayMimeAndKeySystem(memory, mime, mime_size, key_system,
                                       key_system_size);
}

bool H5vccWasmPlayer::canChangeType(v8::Isolate* isolate,
                                    uint32_t current_mime,
                                    uint32_t current_mime_size,
                                    uint32_t new_mime,
                                    uint32_t new_mime_size) {
  WasmPlayerHost* host = imports_->host();
  if (!host) {
    return false;
  }
  GuestMemory memory = imports_->Memory(isolate);
  return host->CanChangeType(memory, current_mime, current_mime_size, new_mime,
                             new_mime_size);
}

void H5vccWasmPlayer::requestWake() {
  if (cobalt::starboard_wasm::EventQueue* queue = imports_->events()) {
    queue->RequestWake();
  }
}

uint32_t H5vccWasmPlayer::pollEvents(v8::Isolate* isolate,
                                     uint32_t events,
                                     uint32_t count) {
  cobalt::starboard_wasm::EventQueue* queue = imports_->events();
  GuestMemory memory = imports_->Memory(isolate);
  uint8_t* out = memory.Array(events, count, sizeof(WasmSbEvent));
  if (!queue || !out) {
    return 0;
  }
  // SAFETY: GuestMemory::Array() checked that `count` records fit at `out`.
  return queue->Poll(
      UNSAFE_BUFFERS(base::span(out, count * sizeof(WasmSbEvent))));
}

void H5vccWasmPlayer::Trace(Visitor* visitor) const {
  visitor->Trace(imports_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
