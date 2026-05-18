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

package dev.cobalt.util;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Defines the constant names for feature switches used in Kimono.
 */
public class JavaSwitches {
  public static final String ENABLE_QUIC = "EnableQUIC";
  public static final String DISABLE_STARTUP_GUARD = "DisableStartupGuard";

  /** flag to re-enable freeze and resume events */
  public static final String ENABLE_FREEZE = "EnableFreeze";

  /** flag to force use IPv4 for system host resolution. */
  public static final String USE_IPV4_FOR_DNS = "UseIPv4ForDNS";

  /** flag to enable fast track mic capture. */
  public static final String ENABLE_COBALT_AUDIO_CAPTURE_FAST_TRACK = "EnableCobaltAudioCaptureFastTrack";

  /** V8 flag to disable decommitting pooled pages. */
  public static final String DISABLE_V8_DECOMMIT_POOLED_PAGES = "DisableV8DecommitPooledPages";

  /** flag to tune compositor offscreen interest area size in pixels. */
  public static final String INTEREST_AREA_SIZE_IN_PIXELS = "InterestAreaSizeInPixels";

  /** flag to tune delay in seconds before reclaiming prepaint tiles when idle. */
  public static final String RECLAIM_DELAY_IN_SECONDS = "ReclaimDelayInSeconds";

  public static List<String> getExtraCommandLineArgs(Map<String, String> javaSwitches) {
    List<String> extraCommandLineArgs = new ArrayList<>();

    if (javaSwitches.containsKey(JavaSwitches.USE_IPV4_FOR_DNS)) {
      extraCommandLineArgs.add("--enable-features=UseIPv4ForDNS");
    }

    if (!javaSwitches.containsKey(JavaSwitches.ENABLE_QUIC)) {
      extraCommandLineArgs.add("--disable-quic");
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_COBALT_AUDIO_CAPTURE_FAST_TRACK)) {
      extraCommandLineArgs.add("--enable-features=CobaltAudioCaptureFastTrack");
    }

    if (javaSwitches.containsKey(JavaSwitches.DISABLE_V8_DECOMMIT_POOLED_PAGES)) {
      extraCommandLineArgs.add("--js-flags=--no-decommit-pooled-pages");
    }

    StringJoiner featureParams = new StringJoiner("/");
    if (javaSwitches.containsKey(JavaSwitches.INTEREST_AREA_SIZE_IN_PIXELS)) {
      String size = javaSwitches.get(JavaSwitches.INTEREST_AREA_SIZE_IN_PIXELS).replaceAll("[^0-9]", "");
      featureParams.add("size_in_pixels/" + size); 
    }

    if (javaSwitches.containsKey(JavaSwitches.RECLAIM_DELAY_IN_SECONDS)) {
      String delay = javaSwitches.get(JavaSwitches.RECLAIM_DELAY_IN_SECONDS).replaceAll("[^0-9]", "");
      featureParams.add("reclaim_delay_s/" + delay); 
    }

    if (featureParams.length() > 0) {
      extraCommandLineArgs.add(
          "--enable-features=SmallerInterestArea:" + featureParams.toString());
    }

    return extraCommandLineArgs;
  }
}
