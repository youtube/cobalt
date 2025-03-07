// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/exoplayer/exoplayer.h"

// static
SbPlayerPrivate* ExoPlayer::CreateInstance() {
  ExoPlayer* exo_player = new ExoPlayer();
  return reinterpret_cast<SbPlayerPrivate*>(exo_player);
}

// static
ExoPlayer* ExoPlayer::GetExoPlayerForSbPlayer(SbPlayer player) {
  return reinterpret_cast<ExoPlayer*>(player);
}

ExoPlayer::~ExoPlayer() {
  bridge_.reset();
}

ExoPlayer::ExoPlayer() {
  bridge_.reset(new ExoPlayerBridge);
}
