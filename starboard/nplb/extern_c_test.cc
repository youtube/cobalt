// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// Ensure that all starboard headers can be included inside extern "C".

extern "C" {
#include "starboard/audio_sink.h"
#include "starboard/configuration.h"
#include "starboard/cpu_features.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/export.h"
#include "starboard/gles.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/microphone.h"
#include "starboard/player.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/speech_synthesis.h"
#include "starboard/storage.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time_zone.h"
#include "starboard/types.h"
#include "starboard/window.h"
}
