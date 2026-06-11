// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_MICROPHONE_LOCAL_AOWS_SERVER_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_MICROPHONE_LOCAL_AOWS_SERVER_H_

namespace starboard {
namespace rdk {
namespace shared {
namespace microphone {

void EnsureLocalAowsServerStarted();
void ClearLocalAowsBufferedAudio();
int ReadLocalAowsAudio(void* out_audio_data, int audio_data_size);

}  // namespace microphone
}  // namespace shared
}  // namespace rdk
}  // namespace starboard

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_MICROPHONE_LOCAL_AOWS_SERVER_H_
