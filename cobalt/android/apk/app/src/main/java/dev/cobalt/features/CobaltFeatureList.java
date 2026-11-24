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

package dev.cobalt.features;

import androidx.annotation.VisibleForTesting;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.chromium.base.BaseFeatures;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlagUtils;
import org.chromium.components.cached_flags.IntCachedFeatureParam;

/**
 * Chromium base::Features and feature params that Cobalt uses in Java.
 *
 * <p>Unlike {@link StarboardFeatureList}, which only reports whether STARBOARD_FEATUREs are enabled
 * once native is initialized, this class exposes Chromium base::Features and their params, and can
 * persist param values for use before native is loaded on the next launch. Features used here must
 * be exposed to Java in //cobalt/common/features/cobalt_feature_map_android.cc.
 *
 * <p>Cached feature params are persisted to SharedPreferences once the native FeatureList is
 * initialized, and applied early in the next launch before native is loaded. As a result, a new
 * value (from Finch or --enable-features) takes effect on the launch after it was received.
 *
 * <p>This is a static utility class that is never instantiated. Its params are static and live as
 * long as the process. Their cached values are stored in the app's SharedPreferences, so they
 * persist across launches.
 *
 * <p>Threading: {@link #applyCachedValuesBeforeNative()} and {@link #cacheNativeValues()} are
 * UI-thread-affine; CobaltActivity calls them on the UI thread during startup. Param values can be
 * read from any thread.
 */
@NullMarked
public final class CobaltFeatureList {
  /**
   * Value of {@link #sLowMemoryDeviceThresholdMb} when LowEndMemoryExperiment doesn't set it. It is
   * not a valid threshold, so it means that the param is not set.
   */
  @VisibleForTesting static final int DEFAULT_LOW_MEMORY_DEVICE_THRESHOLD_MB = Integer.MAX_VALUE;

  /**
   * RAM threshold in MB at or below which the device is considered low-end. Mirrors
   * base::features::kLowMemoryDeviceThresholdMB, which cannot be read from Finch yet when
   * base::SysInfo::IsLowEndDevice() is first called during startup.
   *
   * <p>Cobalt adds --enable-low-end-device-mode by default, which also makes base::SysInfo report
   * at most 512 MB of RAM. When LowEndMemoryExperiment sets this param, Cobalt doesn't add the
   * switch, and base compares the device's RAM against this threshold instead, as in Chromium. See
   * {@link #isLowMemoryDeviceThresholdSet()}.
   *
   * <p>Finch values must be in [1, 4095]. base::FeatureParam<size_t> CHECK-fails on negative
   * values. On the first launch with the param, before it is cached, the switch is still on, so
   * base uses the param as the fake memory limit in SysInfo::AmountOfAvailablePhysicalMemory(),
   * which overflows 32-bit size_t at 4096 or more.
   */
  public static final IntCachedFeatureParam sLowMemoryDeviceThresholdMb =
      new IntCachedFeatureParam(
          CobaltFeatureMap.getInstance(),
          BaseFeatures.LOW_END_MEMORY_EXPERIMENT,
          "LowMemoryDeviceThresholdMB",
          DEFAULT_LOW_MEMORY_DEVICE_THRESHOLD_MB);

  /** All CachedFeatureParams that are persisted for the next launch. */
  private static final List<CachedFeatureParam<?>> sParamsCached =
      Collections.unmodifiableList(Arrays.asList(sLowMemoryDeviceThresholdMb));

  private CobaltFeatureList() {}

  /**
   * Applies the cached feature param values persisted by the previous launch. Must be called in the
   * browser process before the native library is loaded.
   */
  public static void applyCachedValuesBeforeNative() {
    // Otherwise, --enable-low-end-device-mode is on, and SysUtils keeps base's default threshold.
    if (isLowMemoryDeviceThresholdSet()) {
      SysUtils.setLowMemoryDeviceThresholdMb(sLowMemoryDeviceThresholdMb.getValue());
    }
  }

  /**
   * Returns whether LowEndMemoryExperiment sets {@link #sLowMemoryDeviceThresholdMb} to a positive
   * value, as cached by the previous launch. If so, --enable-low-end-device-mode is not added, and
   * base decides whether the device is low-end by comparing its RAM against the threshold. Can be
   * called before the native library is loaded.
   */
  public static boolean isLowMemoryDeviceThresholdSet() {
    int thresholdMb = sLowMemoryDeviceThresholdMb.getValue();
    return thresholdMb > 0 && thresholdMb != DEFAULT_LOW_MEMORY_DEVICE_THRESHOLD_MB;
  }

  /**
   * Persists the current values of the cached feature params for the next launch. Must be called
   * after the native FeatureList is initialized.
   */
  public static void cacheNativeValues() {
    CachedFlagUtils.cacheFeatureParams(Collections.singletonList(sParamsCached));
  }
}
