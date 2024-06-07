// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.media;

/*
class MediaCodecBridgeBuilder {
  @SuppressWarnings("unused")
  @UsedByNative
  public static MediaCodecBridge createAudioMediaCodecBridge(
      long nativeMediaCodecBridge,
      String mime,
      String decoderName,
      int sampleRate,
      int channelCount,
      MediaCrypto crypto,
      @Nullable byte[] configurationData) {
    if (decoderName.equals("")) {
      Log.e(TAG, "Invalid decoder name.");
      return null;
    }
    MediaCodec mediaCodec = null;
    try {
      Log.i(TAG, "Creating \"%s\" decoder.", decoderName);
      mediaCodec = MediaCodec.createByCodecName(decoderName);
    } catch (Exception e) {
      Log.e(TAG, "Failed to create MediaCodec: %s, DecoderName: %s", mime, decoderName, e);
      return null;
    }
    if (mediaCodec == null) {
      return null;
    }
    MediaCodecBridge bridge =
        new MediaCodecBridge(
            nativeMediaCodecBridge, mediaCodec, mime, BitrateAdjustmentTypes.NO_ADJUSTMENT, -1);

    MediaFormat mediaFormat = createAudioFormat(mime, sampleRate, channelCount);

    if (mime.contains("opus")) {
      if (!setOpusConfigurationData(mediaFormat, sampleRate, configurationData)) {
        bridge.release();
        return null;
      }
    } else {
      // TODO: Determine if we should explicitly check the mime for AAC audio before calling
      // setFrameHasADTSHeader(), as more codecs may be supported here in the future.
      setFrameHasADTSHeader(mediaFormat);
    }
    if (!bridge.configureAudio(mediaFormat, crypto, 0)) {
      Log.e(TAG, "Failed to configure audio codec.");
      bridge.release();
      return null;
    }
    if (!bridge.start()) {
      Log.e(TAG, "Failed to start audio codec.");
      bridge.release();
      return null;
    }

    return bridge;
  }
}
*/
