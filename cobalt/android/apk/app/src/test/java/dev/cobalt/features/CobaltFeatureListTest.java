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
  // Default of LowMemoryDeviceThresholdMB on Android.
  private static final int DEFAULT_THRESHOLD_MB = 1024;
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
  }

  @Test
  public void applyCachedValuesBeforeNative_nothingCached_usesDefault() {
    CobaltFeatureList.applyCachedValuesBeforeNative();

    assertThat(CobaltFeatureList.sLowMemoryDeviceThresholdMb.getValue())
        .isEqualTo(DEFAULT_THRESHOLD_MB);
    assertThat(getSysUtilsThresholdMb()).isEqualTo(DEFAULT_THRESHOLD_MB);
  }

  @Test
  public void cacheNativeValues_doesNotChangeValueForThisLaunch() {
    CobaltFeatureList.applyCachedValuesBeforeNative();

    cacheNativeValuesWithThreshold(EXPERIMENT_THRESHOLD_MB);

    assertThat(CobaltFeatureList.sLowMemoryDeviceThresholdMb.getValue())
        .isEqualTo(DEFAULT_THRESHOLD_MB);
    assertThat(getSysUtilsThresholdMb()).isEqualTo(DEFAULT_THRESHOLD_MB);
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

    // Launch 3: back to the default.
    simulateRelaunch();
    CobaltFeatureList.applyCachedValuesBeforeNative();
    assertThat(getSysUtilsThresholdMb()).isEqualTo(DEFAULT_THRESHOLD_MB);
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
