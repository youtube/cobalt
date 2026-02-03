// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import dev.cobalt.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

/** Utility class to log MediaCodec capabilities. */
public class MediaCodecCapabilitiesLogger {
  /** Utility class to save the maximum supported resolution and frame rate of a decoder. */
  static class ResolutionAndFrameRate {
    public ResolutionAndFrameRate(Integer width, Integer height, Double frameRate) {
      this.width = width;
      this.height = height;
      this.frameRate = frameRate;
    }

    public boolean isCovered(Integer width, Integer height, Double frameRate) {
      return this.width >= width && this.height >= height && this.frameRate >= frameRate;
    }

    public Integer width = -1;
    public Integer height = -1;
    public Double frameRate = -1.0;
  }

  /** Returns a string detailing SDR and HDR capabilities of a decoder. */
  private static String getSupportedResolutionsAndFrameRates(
      VideoCapabilities videoCapabilities, boolean isHdrCapable) {
    ArrayList<ArrayList<Integer>> resolutionList =
        new ArrayList<>(
            Arrays.asList(
                new ArrayList<>(Arrays.asList(7680, 4320)),
                new ArrayList<>(Arrays.asList(3840, 2160)),
                new ArrayList<>(Arrays.asList(2560, 1440)),
                new ArrayList<>(Arrays.asList(1920, 1080)),
                new ArrayList<>(Arrays.asList(1280, 720))));
    ArrayList<Double> frameRateList =
        new ArrayList<>(Arrays.asList(60.0, 59.997, 50.0, 48.0, 30.0, 29.997, 25.0, 24.0, 23.997));
    ArrayList<ResolutionAndFrameRate> supportedResolutionsAndFrameRates = new ArrayList<>();
    for (Double frameRate : frameRateList) {
      for (ArrayList<Integer> resolution : resolutionList) {
        boolean isResolutionAndFrameRateCovered = false;
        for (ResolutionAndFrameRate resolutionAndFrameRate : supportedResolutionsAndFrameRates) {
          if (resolutionAndFrameRate.isCovered(resolution.get(0), resolution.get(1), frameRate)) {
            isResolutionAndFrameRateCovered = true;
            break;
          }
        }
        if (videoCapabilities.areSizeAndRateSupported(
            resolution.get(0), resolution.get(1), frameRate)) {
          if (!isResolutionAndFrameRateCovered) {
            supportedResolutionsAndFrameRates.add(
                new ResolutionAndFrameRate(resolution.get(0), resolution.get(1), frameRate));
          }
          continue;
        }
        if (isResolutionAndFrameRateCovered) {
          // This configuration should be covered by a supported configuration, return long form.
          return getLongFormSupportedResolutionsAndFrameRates(
              resolutionList, frameRateList, videoCapabilities, isHdrCapable);
        }
      }
    }
    return convertResolutionAndFrameRatesToString(supportedResolutionsAndFrameRates, isHdrCapable);
  }

  /**
   * Like getSupportedResolutionsAndFrameRates(), but returns the full information for each frame
   * rate and resolution combination.
   */
  private static String getLongFormSupportedResolutionsAndFrameRates(
      ArrayList<ArrayList<Integer>> resolutionList,
      ArrayList<Double> frameRateList,
      VideoCapabilities videoCapabilities,
      boolean isHdrCapable) {
    ArrayList<ResolutionAndFrameRate> supported = new ArrayList<>();
    for (Double frameRate : frameRateList) {
      for (ArrayList<Integer> resolution : resolutionList) {
        if (videoCapabilities.areSizeAndRateSupported(
            resolution.get(0), resolution.get(1), frameRate)) {
          supported.add(
              new ResolutionAndFrameRate(resolution.get(0), resolution.get(1), frameRate));
        }
      }
    }
    return convertResolutionAndFrameRatesToString(supported, isHdrCapable);
  }

  /** Convert a list of ResolutionAndFrameRate to a human readable string. */
  private static String convertResolutionAndFrameRatesToString(
      ArrayList<ResolutionAndFrameRate> supported, boolean isHdrCapable) {
    if (supported.isEmpty()) {
      return "None.";
    }
    String frameRateAndResolutionString = "";
    for (ResolutionAndFrameRate resolutionAndFrameRate : supported) {
      frameRateAndResolutionString +=
          String.format(
              Locale.US,
              "[%d x %d, %.3f fps], ",
              resolutionAndFrameRate.width,
              resolutionAndFrameRate.height,
              resolutionAndFrameRate.frameRate);
    }
    frameRateAndResolutionString += isHdrCapable ? "hdr/sdr" : "sdr";
    return frameRateAndResolutionString;
  }

  private interface CodecFeatureSupported {
    boolean isSupported(String name, CodecCapabilities codecCapabilities);
  }

  static TreeMap<String, CodecFeatureSupported> featureMap;

  private static void ensurefeatureMapInitialized() {
    if (featureMap != null) {
      return;
    }
    featureMap = new TreeMap<>();
    featureMap.put(
        "AdaptivePlayback",
        (name, codecCapabilities) -> {
          return codecCapabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_AdaptivePlayback);
        });
    if (Build.VERSION.SDK_INT >= 29) {
      featureMap.put(
          "FrameParsing",
          (name, codecCapabilities) -> {
            return codecCapabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_FrameParsing);
          });
    }
    if (Build.VERSION.SDK_INT >= 30) {
      featureMap.put(
          "LowLatency",
          (name, codecCapabilities) -> {
            return codecCapabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency);
          });
    }
    if (Build.VERSION.SDK_INT >= 29) {
      featureMap.put(
          "MultipleFrames",
          (name, codecCapabilities) -> {
            return codecCapabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_MultipleFrames);
          });
    }
    if (Build.VERSION.SDK_INT >= 26) {
      featureMap.put(
          "PartialFrame",
          (name, codecCapabilities) -> {
            return codecCapabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_PartialFrame);
          });
    }
    featureMap.put(
        "SecurePlayback",
        (name, codecCapabilities) -> {
          return codecCapabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback);
        });
    featureMap.put(
        "TunneledPlayback",
        (name, codecCapabilities) -> {
          return codecCapabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback);
        });
  }

  private static String getAllFeatureNames() {
    ensurefeatureMapInitialized();
    return featureMap.keySet().toString();
  }

  private static String getSupportedFeaturesAsString(
      String name, CodecCapabilities codecCapabilities) {
    StringBuilder featuresAsString = new StringBuilder();

    ensurefeatureMapInitialized();
    for (Map.Entry<String, CodecFeatureSupported> entry : featureMap.entrySet()) {
      if (entry.getValue().isSupported(name, codecCapabilities)) {
        if (featuresAsString.length() > 0) {
          featuresAsString.append(", ");
        }
        featuresAsString.append(entry.getKey());
      }
    }
    return featuresAsString.toString();
  }

  /**
   * Debug utility function that can be locally added to dump information about all decoders on a
   * particular system.
   */
  public static void dumpAllDecoders() {
    Log.i(
        TAG,
        "COBALT_STARTUP_LOG: ["
            + Thread.currentThread().getName()
            + "] MediaCodecCapabilitiesLogger.dumpAllDecoders START");
    StringBuilder decoderDumpString = new StringBuilder();
    for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      if (info.isEncoder()) {
        continue;
      }
      String isHardwareAccelerated = "unknown";
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        isHardwareAccelerated = info.isHardwareAccelerated() ? "true" : "false";
      }
      for (String supportedType : info.getSupportedTypes()) {
        String name = info.getName();
        decoderDumpString.append(
            String.format(
                Locale.US,
                "name: %s (%s, %s, Hardware accelerated: %s):",
                name,
                supportedType,
                MediaCodecUtil.isCodecDenyListed(name) ? "denylisted" : "not denylisted",
                isHardwareAccelerated));
        CodecCapabilities codecCapabilities = info.getCapabilitiesForType(supportedType);
        VideoCapabilities videoCapabilities = codecCapabilities.getVideoCapabilities();
        String resultName =
            (codecCapabilities.isFeatureSupported(
                        MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback)
                    && !name.endsWith(MediaCodecUtil.getSecureDecoderSuffix()))
                ? (name + MediaCodecUtil.getSecureDecoderSuffix())
                : name;
        boolean isHdrCapable =
            MediaCodecUtil.isHdrCapableVideoDecoder(
                codecCapabilities.getMimeType(), codecCapabilities);
        if (videoCapabilities != null) {
          String frameRateAndResolutionString =
              getSupportedResolutionsAndFrameRates(videoCapabilities, isHdrCapable);
          decoderDumpString.append(
              String.format(
                  Locale.US,
                  "\n\t"
                      + "widths: %s, "
                      + "heights: %s, "
                      + "bitrates: %s, "
                      + "framerates: %s, "
                      + "supported sizes and framerates: %s",
                  videoCapabilities.getSupportedWidths().toString(),
                  videoCapabilities.getSupportedHeights().toString(),
                  videoCapabilities.getBitrateRange().toString(),
                  videoCapabilities.getSupportedFrameRates().toString(),
                  frameRateAndResolutionString));
        }
        String featuresAsString = getSupportedFeaturesAsString(name, codecCapabilities);
        if (featuresAsString.isEmpty()) {
          decoderDumpString.append(" No extra features supported");
        } else {
          decoderDumpString.append("\n\tsupported features: ");
          decoderDumpString.append(featuresAsString);
        }
        decoderDumpString.append("\n");
      }
    }
    Log.v(
        TAG,
        "\n"
            + "==================================================\n"
            + "Full list of decoder features: "
            + getAllFeatureNames()
            + "\nUnsupported features for each codec are not listed\n"
            + decoderDumpString.toString()
            + "==================================================");
  }
}
