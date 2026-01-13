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

#ifndef STARBOARD_ANDROID_SHARED_CONFIGURATE_SEEK_H_
#define STARBOARD_ANDROID_SHARED_CONFIGURATE_SEEK_H_

namespace starboard::android::shared {

// Get flush_decoder_during_reset via
// SetForceFlushDecoderDuringResetForCurrentThread().
bool GetForceFlushDecoderDuringResetForCurrentThread();

// By default, Cobalt recreates MediaCodec when Reset() during Seek().
// Set the following variable to true to force it Flush() MediaCodec
// during Seek().
void SetForceFlushDecoderDuringResetForCurrentThread(
    bool flush_decoder_during_reset);

// Get reset_audio_decoder via SetForceResetAudioDecoderForCurrentThread().
bool GetForceResetAudioDecoderForCurrentThread();

// By default, Cobalt teardowns AudioDecoder during Reset().
// Set the following variable to true to force it reset audio decoder
// during Reset(). This should be enabled with
// SetForceFlushDecoderDuringResetForCurrentThread.
void SetForceResetAudioDecoderForCurrentThread(bool reset_audio_decoder);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_CONFIGURATE_SEEK_H_
