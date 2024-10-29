// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.util.Range;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Cache manager for maintaining a video decoder cache. Threadsafe. */
public class VideoDecoderCache {
  /**
   * Cache entry in VideoDecoderCache. Cached expensive API calls that are repeatedly made while the
   * H5 player determines codec support.
   */
  public static class CachedDecoder {
    CachedDecoder(MediaCodecInfo info, CodecCapabilities codecCapabilities, String mimeType) {
      this.info = info;
      this.mimeType = mimeType;
      this.codecCapabilities = codecCapabilities;
      this.videoCapabilities = codecCapabilities.getVideoCapabilities();
      this.supportedWidths = videoCapabilities.getSupportedWidths();
      this.supportedHeights = videoCapabilities.getSupportedHeights();
      this.bitrateRange = videoCapabilities.getBitrateRange();
    }

    public MediaCodecInfo info;
    public String mimeType;
    public CodecCapabilities codecCapabilities;
    public VideoCapabilities videoCapabilities;
    public Range<Integer> supportedWidths;
    public Range<Integer> supportedHeights;
    public Range<Integer> bitrateRange;
  }

  private static final int DEFAULT_CACHE_TTL_MILLIS = 1000;

  private static Map<String, List<CachedDecoder>> cache = new HashMap<>();
  private static long lastCacheUpdateAt = 0;

  private static synchronized boolean isExpired(int cacheTtlOverride) {
    long cacheTtl = cacheTtlOverride >= 0 ? cacheTtlOverride : DEFAULT_CACHE_TTL_MILLIS;
    return System.currentTimeMillis() - lastCacheUpdateAt >= cacheTtl;
  }

  private static void refreshDecoders() {
    cache.clear();
    // Note: MediaCodecList is sorted by the framework such that the best decoders come first.
    // This order is maintained in the cache.
    for (MediaCodecInfo codecInfo : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      // Don't cache encoders.
      if (codecInfo.isEncoder()) {
        continue;
      }
      for (String mimeType : codecInfo.getSupportedTypes()) {
        CodecCapabilities codecCapabilities = codecInfo.getCapabilitiesForType(mimeType);
        // Don't cache decoders that can't do video.
        if (codecCapabilities.getVideoCapabilities() == null) {
          continue;
        }
        List<CachedDecoder> decoders = cache.get(mimeType);
        if (decoders == null) {
          decoders = new ArrayList<>();
          cache.put(mimeType, decoders);
        }
        decoders.add(new CachedDecoder(codecInfo, codecCapabilities, mimeType));
      }
    }
    lastCacheUpdateAt = System.currentTimeMillis();
  }

  /**
   * Returns a list of decoders that are cabable of decoding the media denoted by {@code mimeType}.
   *
   * @param mimeType The mime type the returned decoders should be able to decode.
   * @param cacheTtlOverride Overrides the default cache TTL if passed a non-negative number. 0
   *     disables the cache entirely.
   * @return A list of decoders.
   */
  static List<CachedDecoder> getCachedDecoders(String mimeType, int cacheTtlOverride) {
    synchronized (cache) {
      if (isExpired(cacheTtlOverride)) {
        refreshDecoders();
      }
      return cache.containsKey(mimeType)
          ? cache.get(mimeType)
          : Collections.<CachedDecoder>emptyList();
    }
  }
}
