// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureMap;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.task.test.PausedExecutorTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;
import org.chromium.components.cached_flags.CachedFlagsSafeMode.Behavior;

import java.util.Arrays;

/** Unit Tests for {@link CachedFlagsSafeMode}, the Safe Mode mechanism for {@link CachedFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedFeatureFlagsSafeModeUnitTest {
    @Rule public PausedExecutorTestRule mExecutorRule = new PausedExecutorTestRule();
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    private static final String CRASHY_FEATURE = "CrashyFeature";
    private static final String OK_FEATURE = "OkFeature";
    private static final boolean CRASHY_FEATURE_DEFAULT = false;
    private static final boolean OK_FEATURE_DEFAULT = false;

    private static final String BOOL_PARAM_NAME = "BoolParam";
    private static final String INT_PARAM_NAME = "IntParam";
    private static final String DOUBLE_PARAM_NAME = "DoubleParam";
    private static final String STRING_PARAM_NAME = "StringParam";
    private static final boolean BOOL_PARAM_DEFAULT = false;
    private static final int INT_PARAM_DEFAULT = 111;
    private static final double DOUBLE_PARAM_DEFAULT = 55.5;
    private static final String STRING_PARAM_DEFAULT = "foo";
    private static final boolean BOOL_PARAM_NATIVE_1 = false;
    private static final int INT_PARAM_NATIVE_1 = 222;
    private static final double DOUBLE_PARAM_NATIVE_1 = 66.5;
    private static final String STRING_PARAM_NATIVE_1 = "bar";
    private static final boolean BOOL_PARAM_NATIVE_2 = true;
    private static final int INT_PARAM_NATIVE_2 = 333;
    private static final double DOUBLE_PARAM_NATIVE_2 = 77.5;
    private static final String STRING_PARAM_NATIVE_2 = "baz";

    @Mock private FeatureMap mFeatureMap;
    private CachedFlag mCrashyFeature;
    private CachedFlag mOkFeature;
    private BooleanCachedFieldTrialParameter mBoolParam;
    private IntCachedFieldTrialParameter mIntParam;
    private DoubleCachedFieldTrialParameter mDoubleParam;
    private StringCachedFieldTrialParameter mStringParam;

    @Before
    public void setUp() {
        mCrashyFeature = new CachedFlag(mFeatureMap, CRASHY_FEATURE, CRASHY_FEATURE_DEFAULT);
        mOkFeature = new CachedFlag(mFeatureMap, OK_FEATURE, OK_FEATURE_DEFAULT);
        mBoolParam =
                new BooleanCachedFieldTrialParameter(
                        mFeatureMap, OK_FEATURE, BOOL_PARAM_NAME, BOOL_PARAM_DEFAULT);
        mIntParam =
                new IntCachedFieldTrialParameter(
                        mFeatureMap, OK_FEATURE, INT_PARAM_NAME, INT_PARAM_DEFAULT);
        mDoubleParam =
                new DoubleCachedFieldTrialParameter(
                        mFeatureMap, OK_FEATURE, DOUBLE_PARAM_NAME, DOUBLE_PARAM_DEFAULT);
        mStringParam =
                new StringCachedFieldTrialParameter(
                        mFeatureMap, OK_FEATURE, STRING_PARAM_NAME, STRING_PARAM_DEFAULT);

        CachedFlagsSafeMode.getInstance().enableForTesting();
        clearMemory();
    }

    @After
    public void tearDown() {
        clearMemory();
    }

    @Test
    public void testTwoCrashesInARow_engageSafeMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
        endCleanRun(
                false,
                true,
                BOOL_PARAM_NATIVE_1,
                INT_PARAM_NATIVE_1,
                DOUBLE_PARAM_NATIVE_1,
                STRING_PARAM_NATIVE_1);
        // Safe values became false/false/defaults.
        // Cached values became false/true/native1.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false/defaults.
        // Cached values are false/true/native1, from previous run.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCleanRun(
                true,
                true,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Safe values became false/true/native1.
        // Cached values became true(crashy)/true/native2.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/true/native1.
        // Cached values remain true(crashy)/true/native2 and are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native2.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/true/native1.
        // Cached values remain true(crashy)/true/native2 and are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native2.

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        // Safe values are false/true/native1, and are used during this run.
        // Cached values remain true(crashy)/true/native2, but are not used because Safe Mode is
        // engaged.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCleanRun(
                true,
                false,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Cached values became true(crashy)/false/native2, cached from native.

        startRun();
        // Second run of Safe Mode.
        // Safe values are false/true/native1, and are used during this run.
        // Cached values true(crashy)/false/native2 are used, cached from native last run, but are
        // not used because Safe Mode is engaged.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCleanRun(
                false,
                false,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Cached values became false/false/native2, cached from native.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are still false/true/native1.
        // Cached values false/false/native2 are used, cached from native last run.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertFalse(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
    }

    @Test
    public void testSafeModeFetchesBadConfig_keepsStreak() {
        startRun();
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Second run of safe mode.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is back directly to 2. Engage Safe Mode.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
    }

    @Test
    public void testSafeModeFetchesGoodConfig_decreasesStreak() {
        startRun();
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Second run of safe mode.
        assertEquals(
                Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCleanRunCachingIrrelevantValues();

        startRun();
        // Crash streak is down to 0. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
    }

    @Test
    public void testTwoCrashesInterrupted_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
        endCleanRun(
                true,
                true,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Safe values became false/false/defaults.
        // Cached values became true(flaky)/true/native2.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false/defaults.
        // Cached values are true(flaky)/true/native2.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native2.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/false/defaults.
        // Cached values are true(flaky)/true/native2.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        // Cached values are the flaky ones cached from native.
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCleanRun(
                true,
                true,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Safe values became true(flaky)/true/native2.
        // Cached values remain true(flaky)/true/native2.

        startRun();
        // Crash streak is 0, do not engage, use flaky values.
        // Safe values are true(flaky)/true/native2.
        // Cached values are true(flaky)/true/native2.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
    }

    /**
     * Tests that decrementing the crash streak to account for an aborted run prevents Safe Mode
     * from engaging.
     */
    @Test
    public void testTwoFREs_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
        endFirstRunWithKill();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
        endFirstRunWithKill();

        startRun();
        // Crash streak is 0, do not engage, use flaky values.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
    }

    @Test
    public void testTwoCrashesInARow_engageSafeModeWithoutSafeValues() {
        // Simulate a cache without writing safe values. This happens before Safe Mode was
        // implemented and will become rare as clients start writing safe values.
        // Cache a crashy value.
        when(mFeatureMap.isEnabledInNative(CRASHY_FEATURE)).thenReturn(true);
        when(mFeatureMap.isEnabledInNative(OK_FEATURE)).thenReturn(true);
        when(mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        eq(OK_FEATURE), eq(BOOL_PARAM_NAME), anyBoolean()))
                .thenReturn(BOOL_PARAM_NATIVE_1);
        when(mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        eq(OK_FEATURE), eq(INT_PARAM_NAME), anyInt()))
                .thenReturn(INT_PARAM_NATIVE_1);
        when(mFeatureMap.getFieldTrialParamByFeatureAsDouble(
                        eq(OK_FEATURE), eq(DOUBLE_PARAM_NAME), anyDouble()))
                .thenReturn(DOUBLE_PARAM_NATIVE_1);
        when(mFeatureMap.getFieldTrialParamByFeature(OK_FEATURE, STRING_PARAM_NAME))
                .thenReturn(STRING_PARAM_NATIVE_1);
        CachedFlagUtils.cacheNativeFlags(Arrays.asList(mCrashyFeature, mOkFeature));
        CachedFlagUtils.cacheFieldTrialParameters(
                Arrays.asList(mBoolParam, mIntParam, mDoubleParam, mStringParam));

        clearMemory();
        // Cached values became true(crashy)/true/native1.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // Cached values are true(crashy)/true/native1.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native1.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // There are no safe values.
        // Cached values are true(crashy)/true/native1.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native1.

        startRun();
        // Crash streak is 2. Engage Safe Mode without safe values.
        // There are no safe values.
        // Cached values are true(crashy)/true/native1, but the default values false/false/defaults
        // are returned since Safe Mode is falling back to default.
        assertEquals(
                Behavior.ENGAGED_WITHOUT_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
    }

    @Test
    public void testTwoCrashesInARow_engageSafeModeIgnoringOutdated() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false/defaults are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
        endCleanRun(
                false,
                true,
                BOOL_PARAM_NATIVE_1,
                INT_PARAM_NATIVE_1,
                DOUBLE_PARAM_NATIVE_1,
                STRING_PARAM_NATIVE_1);
        // Safe values became false/false/defaults.
        // Cached values became false/true/native1.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false/defaults.
        // Cached values are false/true/native1, from previous run.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCleanRun(
                true,
                true,
                BOOL_PARAM_NATIVE_2,
                INT_PARAM_NATIVE_2,
                DOUBLE_PARAM_NATIVE_2,
                STRING_PARAM_NATIVE_2);
        // Safe values became false/true/native1.
        // Cached values became true(crashy)/true/native2.

        // Pretend safe values are from an older version
        CachedFlagsSafeMode.getSafeValuePreferences()
                .edit()
                .putString(CachedFlagsSafeMode.PREF_SAFE_VALUES_VERSION, "1.0.0.0")
                .apply();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/true/native1, but from another version.
        // Cached values are true(crashy)/true/native2.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native2.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/true/native1, but from another version.
        // Cached values are true(crashy)/true/native2.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative2();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native2.

        startRun();
        assertEquals(
                Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        // Crash streak is 2. Engage Safe Mode with obsolete safe values.
        // Safe values are false/true/native1, but from another version.
        // Cached values are true(crashy)/true/native2, but the default values false/false/defaults
        // are returned since Safe Mode is falling back to default.
        assertEquals(CRASHY_FEATURE_DEFAULT, mCrashyFeature.isEnabled());
        assertEquals(OK_FEATURE_DEFAULT, mOkFeature.isEnabled());
        assertCachedParamsEqualDefaults();
    }

    @Test
    public void testMultipleStartCheckpoints_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached values, so the defaults false/false are used.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertFalse(mCrashyFeature.isEnabled());
        assertFalse(mOkFeature.isEnabled());
        endCleanRun(
                true,
                true,
                BOOL_PARAM_NATIVE_1,
                INT_PARAM_NATIVE_1,
                DOUBLE_PARAM_NATIVE_1,
                STRING_PARAM_NATIVE_1);
        // Safe values became false/false/defaults.
        // Cached values became true(crashy)/true/native1.

        startRun();
        startRun();
        startRun();
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false/defaults.
        // Cached values are true(crashy)/true/native1.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
        assertTrue(mCrashyFeature.isEnabled());
        assertTrue(mOkFeature.isEnabled());
        assertCachedParamsEqualNative1();
        endCrashyRun();
        // Cached values remain true(crashy)/true/native1.

        startRun();
        // Crash streak is 1, despite the multiple startRun() calls above. Do not engage Safe Mode.
        assertEquals(
                Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFlagsSafeMode.getInstance().getBehaviorForTesting());
    }

    private void startRun() {
        // Enter safe mode or not before the start checkpoint, since that's what happens and should
        // be supported.
        mCrashyFeature.isEnabled();

        CachedFlagsSafeMode.getInstance().onStartOrResumeCheckpoint();

        // Only flags and params that are checked before native flags are cached need to be switched
        // to safe values.
        mBoolParam.getValue();
        mIntParam.getValue();
        mDoubleParam.getValue();
        mStringParam.getValue();
    }

    private void endFirstRunWithKill() {
        CachedFlagsSafeMode.getInstance().onPauseCheckpoint();
        clearMemory();
    }

    private void endCrashyRun() {
        clearMemory();
    }

    private void endCleanRunCachingIrrelevantValues() {
        endCleanRun(
                false,
                false,
                BOOL_PARAM_NATIVE_1,
                INT_PARAM_NATIVE_1,
                DOUBLE_PARAM_NATIVE_1,
                STRING_PARAM_NATIVE_1);
    }

    private void endCleanRun(
            boolean crashyFeatureValue,
            boolean okFeatureValue,
            boolean boolParamValue,
            int intParamValue,
            double doubleParamValue,
            String stringParamValue) {
        when(mFeatureMap.isEnabledInNative(CRASHY_FEATURE)).thenReturn(crashyFeatureValue);
        when(mFeatureMap.isEnabledInNative(OK_FEATURE)).thenReturn(okFeatureValue);
        when(mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        eq(OK_FEATURE), eq(BOOL_PARAM_NAME), anyBoolean()))
                .thenReturn(boolParamValue);
        when(mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        eq(OK_FEATURE), eq(INT_PARAM_NAME), anyInt()))
                .thenReturn(intParamValue);
        when(mFeatureMap.getFieldTrialParamByFeatureAsDouble(
                        eq(OK_FEATURE), eq(DOUBLE_PARAM_NAME), anyDouble()))
                .thenReturn(doubleParamValue);
        when(mFeatureMap.getFieldTrialParamByFeature(OK_FEATURE, STRING_PARAM_NAME))
                .thenReturn(stringParamValue);

        CachedFlagUtils.cacheNativeFlags(Arrays.asList(mCrashyFeature, mOkFeature));
        CachedFlagUtils.cacheFieldTrialParameters(
                Arrays.asList(mBoolParam, mIntParam, mDoubleParam, mStringParam));

        CachedFlagsSafeMode.getInstance().onEndCheckpoint();
        mExecutorRule.runAllBackgroundAndUi();

        assertTrue(
                CachedFlagsSafeMode.getSafeValuePreferences()
                        .contains("Chrome.Flags.CachedFlag.CrashyFeature"));

        clearMemory();
    }

    private void assertCachedParamsEqualDefaults() {
        assertEquals(BOOL_PARAM_DEFAULT, mBoolParam.getValue());
        assertEquals(INT_PARAM_DEFAULT, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_DEFAULT, mDoubleParam.getValue(), 1e-10);
        assertEquals(STRING_PARAM_DEFAULT, mStringParam.getValue());
    }

    private void assertCachedParamsEqualNative1() {
        assertEquals(BOOL_PARAM_NATIVE_1, mBoolParam.getValue());
        assertEquals(INT_PARAM_NATIVE_1, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_NATIVE_1, mDoubleParam.getValue(), 1e-10);
        assertEquals(STRING_PARAM_NATIVE_1, mStringParam.getValue());
    }

    private void assertCachedParamsEqualNative2() {
        assertEquals(BOOL_PARAM_NATIVE_2, mBoolParam.getValue());
        assertEquals(INT_PARAM_NATIVE_2, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_NATIVE_2, mDoubleParam.getValue(), 1e-10);
        assertEquals(STRING_PARAM_NATIVE_2, mStringParam.getValue());
    }

    private static void clearMemory() {
        ValuesReturned.clearForTesting();
        CachedFlagsSafeMode.getInstance().clearMemoryForTesting();
    }
}
