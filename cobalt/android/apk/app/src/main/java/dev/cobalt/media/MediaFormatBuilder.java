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

import static dev.cobalt.media.Log.TAG;

import android.media.MediaFormat;
import androidx.annotation.Nullable;
import dev.cobalt.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;

class MediaFormatBuilder {
  public static void setCodecSpecificData(MediaFormat format, byte[][] csds) {
    // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
    // csd-1, and so on. See:
    // http://developer.android.com/reference/android/media/MediaCodec.html for details.
    for (int i = 0; i < csds.length; ++i) {
      if (csds[i].length == 0) {
        continue;
      }
      String name = "csd-" + i;
      format.setByteBuffer(name, ByteBuffer.wrap(csds[i]));
    }
  }

  public static MediaFormat createAudioFormat(
      String mime, int sampleRate, int channelCount, byte[][] csds, boolean frameHasAdtsHeader) {
    MediaFormat format = MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
    setCodecSpecificData(format, csds);
    if (frameHasAdtsHeader) {
      format.setInteger(MediaFormat.KEY_IS_ADTS, 1);
    }
    return format;
  }

  public static byte[][] starboardParseOpusConfigurationData(
      int sampleRate, @Nullable byte[] configurationData) {
    final int minOpusInitializationDataBufferSize = 19;
    final long nanosecondsInOneSecond = 1000000000L;
    // 3840 is the default seek pre-roll samples used by ExoPlayer:
    // https://github.com/google/ExoPlayer/blob/0ba317b1337eaa789f05dd6c5241246478a3d1e5/library/common/src/main/java/com/google/android/exoplayer2/audio/OpusUtil.java#L30.
    final int defaultSeekPreRollSamples = 3840;
    if (configurationData == null
        || configurationData.length < minOpusInitializationDataBufferSize) {
      Log.e(
          TAG,
          "Failed to configure Opus audio codec. "
              + (configurationData == null
                  ? "|configurationData| is null."
                  : String.format(
                      Locale.US,
                      "Configuration data size (%d) is less than the required size (%d).",
                      configurationData.length,
                      minOpusInitializationDataBufferSize)));
      return null;
    }
    // Both the number of samples to skip from the beginning of the stream and the amount of time
    // to pre-roll when seeking must be specified when configuring the Opus decoder. Logic adapted
    // from ExoPlayer:
    // https://github.com/google/ExoPlayer/blob/0ba317b1337eaa789f05dd6c5241246478a3d1e5/library/common/src/main/java/com/google/android/exoplayer2/audio/OpusUtil.java#L52.
    int preSkipSamples = ((configurationData[11] & 0xFF) << 8) | (configurationData[10] & 0xFF);
    long preSkipNanos = (preSkipSamples * nanosecondsInOneSecond) / sampleRate;
    long seekPreRollNanos =
        (defaultSeekPreRollSamples * nanosecondsInOneSecond) / sampleRate;
    return new byte[][] {
      configurationData,
      ByteBuffer.allocate(8).order(ByteOrder.nativeOrder()).putLong(preSkipNanos).array(),
      ByteBuffer.allocate(8).order(ByteOrder.nativeOrder()).putLong(seekPreRollNanos).array(),
    };
  }
}
