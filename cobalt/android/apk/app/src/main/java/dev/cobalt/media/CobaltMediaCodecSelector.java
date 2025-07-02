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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import androidx.media3.exoplayer.mediacodec.MediaCodecInfo;
import androidx.media3.exoplayer.mediacodec.MediaCodecSelector;
import androidx.media3.exoplayer.mediacodec.MediaCodecUtil;

import dev.cobalt.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class CobaltMediaCodecSelector implements MediaCodecSelector {

    @Override
    public List<MediaCodecInfo> getDecoderInfos(
            String mimeType, boolean requiresSecureDecoder, boolean requiresTunnelingDecoder) {

        List<MediaCodecInfo> defaultDecoderInfos = null;
        try {
            defaultDecoderInfos =
                    androidx.media3.exoplayer.mediacodec.MediaCodecUtil.getDecoderInfos(mimeType, requiresSecureDecoder, requiresTunnelingDecoder);
        } catch (MediaCodecUtil.DecoderQueryException e) {
            Log.i(TAG, String.format("MediaCodecUtil.getDecoderInfos() error %s", e.toString()));
            return defaultDecoderInfos;
            }
            // Skip video decoder filtering for emulators.
//                if (IsEmulator.isEmulator()) {
//                    Log.i(TAG, "Allowing all available decoders for emulator");
//                    return defaultDecoderInfos;
//                }

            List<MediaCodecInfo> filteredDecoderInfos = new ArrayList<>();

            if (mimeType.startsWith("video/")) {
                for (MediaCodecInfo decoderInfo : defaultDecoderInfos) {
                    if (!isSoftwareDecoder(decoderInfo)) {
                        Log.i(TAG, String.format("Adding decoder %s", decoderInfo.name));
                        filteredDecoderInfos.add(decoderInfo);
                        continue;
                    }

                    Log.i(TAG, String.format("Filtering software decoder %s", decoderInfo.name));
                }
                return filteredDecoderInfos.isEmpty() ? defaultDecoderInfos : filteredDecoderInfos; // Fallback to default if no hardware decoders found
            } else {
                // Return default decoders for non-video.
                return defaultDecoderInfos;
            }
    }

    private static boolean isSoftwareDecoder(MediaCodecInfo codecInfo) {
        String name = codecInfo.name.toLowerCase(Locale.ROOT);
        // This is taken from libstagefright/OMXCodec.cpp for pre codec2.
        if (name.startsWith("omx.google.")) {
            return true;
        }

        // Codec2 names sw decoders this way.
        // See hardware/google/av/codec2/vndk/C2Store.cpp.
        if (name.startsWith("c2.google.") || name.startsWith("c2.android.")) {
            return true;
        }

        return false;
    }

//    @Override
//    public MediaCodecInfo getPassthroughDecoderInfo() {
//        // You might want to keep the default behavior for passthrough decoders
//        return MediaCodecUtil.getPassthroughDecoderInfo();
//    }
}
