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

import static com.google.common.truth.Truth.assertThat;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.SysUtils;
import org.chromium.base.cached_flags.ValuesReturned;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

/**
 * Tests for {@link CobaltFeatureList}.
 *
 * <p>Tests simulate one or more launches. In each launch, {@link
 * CobaltFeatureList#applyCachedValuesBeforeNative()} runs before native is loaded, and {@link
 * CobaltFeatureList#cacheNativeValues()} runs once the native FeatureList is initialized.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CobaltFeatureListTest {
  // Value of LowMemoryDeviceThresholdMB when LowEndMemoryExperiment doesn't set it.
  private static final int DEFAULT_THRESHOLD_MB =
      CobaltFeatureList.DEFAULT_LOW_MEMORY_DEVICE_THRESHOLD_MB;
  // Value of LowMemoryDeviceThresholdMB set by Finch or --enable-features.
  private static final int EXPERIMENT_THRESHOLD_MB = 2048;
  // Never set by CobaltFeatureList, so tests can tell whether SysUtils was updated.
  private static final int UNSET_THRESHOLD_MB = -1;

  private boolean mOriginalDisableNativeForTesting;
  private int mOriginalSysUtilsThresholdMb;

  @Before
  public void setUp() {
    // Use fresh SharedPreferences, so nothing was cached by a previous launch.
    ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
    ValuesReturned.clearForTesting();

    // Native can't be loaded in Robolectric. With native disabled, FeatureMap returns the param
    // default unless a test overrides it, like native does when the param is not set.
    mOriginalDisableNativeForTesting = FeatureList.getDisableNativeForTesting();
    FeatureList.setDisableNativeForTesting(true);

    mOriginalSysUtilsThresholdMb = getSysUtilsThresholdMb();
    SysUtils.setLowMemoryDeviceThresholdMb(UNSET_THRESHOLD_MB);
  }

  @After
  public void tearDown() {
    // RobolectricTestRunner doesn't run ResettersForTesting, so reset static state here.
    FeatureOverrides.removeAllIncludingAnnotations();
    ValuesReturned.clearForTesting();
    FeatureList.setDisableNativeForTesting(mOriginalDisableNativeForTesting);
    SysUtils.setLowMemoryDeviceThresholdMb(mOriginalSysUtilsThresholdMb);
    ReflectionHelpers.setStaticField(SysUtils.class, "sAmountOfPhysicalMemoryKB", null);
    ReflectionHelpers.setStaticField(SysUtils.class, "sLowEndDevice", null);
  }

  @Test
  public void isLowMemoryDeviceThresholdSet_nothingCached_false() {
    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isFalse();
  }

  @Test
  public void isLowMemoryDeviceThresholdSet_cachedByPreviousLaunch_true() {
    CobaltFeatureList.applyCachedValuesBeforeNative();
    cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);
    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isFalse();

    simulateRelaunch();

    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isTrue();
  }

  @Test
  public void isLowMemoryDeviceThresholdSet_nonPositiveThreshold_false() {
    CobaltFeatureList.sLowMemoryDeviceThresholdMb.setForTesting(0);
    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isFalse();

    CobaltFeatureList.sLowMemoryDeviceThresholdMb.setForTesting(-1);
    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isFalse();
  }

  @Test
  public void applyCachedValuesBeforeNative_javaIsLowEndDeviceUsesCachedThreshold() {
    // No --enable-low-end-device-mode, so SysUtils compares RAM against the threshold.
    CommandLine.resetForTesting(true);
    try {
      ReflectionHelpers.setStaticField(SysUtils.class, "sAmountOfPhysicalMemoryKB", 4096 * 1024);
      CobaltFeatureList.applyCachedValuesBeforeNative();
      cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);

      simulateRelaunch();
      CobaltFeatureList.applyCachedValuesBeforeNative();

      // 4096 MB is above the cached threshold, so Java agrees with native that the device is not
      // low-end, instead of comparing against SysUtils.LOW_MEMORY_DEVICE_THRESHOLD_MB.
      ReflectionHelpers.setStaticField(SysUtils.class, "sLowEndDevice", null);
      assertThat(SysUtils.isLowEndDevice()).isFalse();

      ReflectionHelpers.setStaticField(
          SysUtils.class, "sAmountOfPhysicalMemoryKB", EXPERIMENT_THRESHOLD_MB * 1024);
      ReflectionHelpers.setStaticField(SysUtils.class, "sLowEndDevice", null);
      assertThat(SysUtils.isLowEndDevice()).isTrue();
    } finally {
      CommandLine.resetForTesting(false);
    }
  }

  @Test
  public void applyCachedValuesBeforeNative_nothingCached_keepsSysUtilsThreshold() {
    CobaltFeatureList.applyCachedValuesBeforeNative();

    assertThat(CobaltFeatureList.sLowMemoryDeviceThresholdMb.getValue())
        .isEqualTo(DEFAULT_THRESHOLD_MB);
    assertThat(getSysUtilsThresholdMb()).isEqualTo(UNSET_THRESHOLD_MB);
  }

  @Test
  public void cacheNativeValues_doesNotChangeValueForThisLaunch() {
    CobaltFeatureList.applyCachedValuesBeforeNative();

    cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);

    assertThat(CobaltFeatureList.sLowMemoryDeviceThresholdMb.getValue())
        .isEqualTo(DEFAULT_THRESHOLD_MB);
    assertThat(getSysUtilsThresholdMb()).isEqualTo(UNSET_THRESHOLD_MB);
  }

  @Test
  public void applyCachedValuesBeforeNative_usesValueCachedByPreviousLaunch() {
    CobaltFeatureList.applyCachedValuesBeforeNative();
    cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);

    simulateRelaunch();
    CobaltFeatureList.applyCachedValuesBeforeNative();

    assertThat(CobaltFeatureList.sLowMemoryDeviceThresholdMb.getValue())
        .isEqualTo(EXPERIMENT_THRESHOLD_MB);
    assertThat(getSysUtilsThresholdMb()).isEqualTo(EXPERIMENT_THRESHOLD_MB);
  }

  @Test
  public void applyCachedValuesBeforeNative_paramRemoved_usesDefaultOnNextLaunch() {
    // Launch 1: the param is set.
    CobaltFeatureList.applyCachedValuesBeforeNative();
    cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);

    // Launch 2: the cached value is used, but the param is no longer set.
    simulateRelaunch();
    CobaltFeatureList.applyCachedValuesBeforeNative();
    assertThat(getSysUtilsThresholdMb()).isEqualTo(EXPERIMENT_THRESHOLD_MB);
    CobaltFeatureList.cacheNativeValues();

    // Launch 3: back to the default, so SysUtils is not updated.
    simulateRelaunch();
    CobaltFeatureList.applyCachedValuesBeforeNative();
    assertThat(CobaltFeatureList.isLowMemoryDeviceThresholdSet()).isFalse();
    assertThat(getSysUtilsThresholdMb()).isEqualTo(UNSET_THRESHOLD_MB);
  }

  /** Calls cacheNativeValues() as if native had {@code thresholdMb} as the param value. */
  private static void cacheNativeValuesWithThreshold(int thresholdMb) {
    // A test override stands in for the native value. Remove it after caching, because overrides
    // also take precedence over the value used for this launch.
    CobaltFeatureList.sLowMemoryDeviceThresholdMb.setForTesting(thresholdMb);
    CobaltFeatureList.cacheNativeValues();
    FeatureOverrides.removeAllIncludingAnnotations();
  }

  /** Simulates a cold start: in-memory state is reset, but SharedPreferences are kept. */
  private static void simulateRelaunch() {
    ValuesReturned.clearForTesting();
    SysUtils.setLowMemoryDeviceThresholdMb(UNSET_THRESHOLD_MB);
  }

  /** Returns the threshold that native base::SysInfo::IsLowEndDevice() reads through JNI. */
  private static int getSysUtilsThresholdMb() {
    // SysUtils has no public getter, so call the @CalledByNative method that native uses.
    return ReflectionHelpers.callStaticMethod(SysUtils.class, "getLowMemoryDeviceThresholdMb");
  }
}
