// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

import java.util.List;
import java.util.Map;

/**
 * Unit Tests for {@link CachedFieldTrialParameter} and its subclasses.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedFieldTrialParameterUnitTest {
    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    public ChromeFeatureList.Natives mChromeFeatureListNativesMock;

    private static final String FEATURE_A = "FeatureA";

    private static final String STRING_PARAM_NAME = "ParamString";
    private static final String STRING_PARAM_DEFAULT = "default";
    private static final String STRING_PARAM_NATIVE = "native";
    private static final String STRING_PARAM_TEST_OVERRIDE = "override";
    private static final String STRING_PARAM_BAD = "bad";

    private static final String BOOLEAN_PARAM_NAME = "ParamBoolean";
    private static final boolean BOOLEAN_PARAM_DEFAULT = false;
    private static final boolean BOOLEAN_PARAM_NATIVE = true;
    private static final String BOOLEAN_PARAM_NATIVE_STRING = "true";
    private static final boolean BOOLEAN_PARAM_TEST_OVERRIDE = true;
    private static final String BOOLEAN_PARAM_TEST_OVERRIDE_STRING = "true";
    private static final String BOOLEAN_PARAM_BAD = "false";

    private static final String INT_PARAM_NAME = "ParamInt";
    private static final int INT_PARAM_DEFAULT = 1;
    private static final int INT_PARAM_NATIVE = 2;
    private static final String INT_PARAM_NATIVE_STRING = "2";
    private static final int INT_PARAM_TEST_OVERRIDE = 3;
    private static final String INT_PARAM_TEST_OVERRIDE_STRING = "3";
    private static final String INT_PARAM_BAD = "9";

    private static final String DOUBLE_PARAM_NAME = "ParamDouble";
    private static final double DOUBLE_PARAM_DEFAULT = 1.0;
    private static final double DOUBLE_PARAM_NATIVE = 2.0;
    private static final String DOUBLE_PARAM_NATIVE_STRING = "2.0";
    private static final double DOUBLE_PARAM_TEST_OVERRIDE = 3.0;
    private static final String DOUBLE_PARAM_TEST_OVERRIDE_STRING = "3.0";
    private static final String DOUBLE_PARAM_BAD = "9.0";

    // Different in that the native value will be "", which makes the default be cached.
    private static final String STRING2_PARAM_NAME = "ParamString2";
    private static final String STRING2_PARAM_DEFAULT = "default2";
    private static final String STRING2_PARAM_NATIVE = "";
    private static final String STRING2_PARAM_TEST_OVERRIDE = "override2";
    private static final String STRING2_PARAM_BAD = "bad2";

    private static final StringCachedFieldTrialParameter STRING_PARAM =
            new StringCachedFieldTrialParameter(FEATURE_A, STRING_PARAM_NAME, STRING_PARAM_DEFAULT);
    private static final BooleanCachedFieldTrialParameter BOOLEAN_PARAM =
            new BooleanCachedFieldTrialParameter(
                    FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_DEFAULT);
    private static final IntCachedFieldTrialParameter INT_PARAM =
            new IntCachedFieldTrialParameter(FEATURE_A, INT_PARAM_NAME, INT_PARAM_DEFAULT);
    private static final DoubleCachedFieldTrialParameter DOUBLE_PARAM =
            new DoubleCachedFieldTrialParameter(FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_DEFAULT);
    private static final StringCachedFieldTrialParameter STRING2_PARAM =
            new StringCachedFieldTrialParameter(
                    FEATURE_A, STRING2_PARAM_NAME, STRING2_PARAM_DEFAULT);

    private static final String FEATURE_B = "FeatureB";

    // clang-format off
    private static final String[] ALL_PARAMS_NATIVE = new String[] {
            STRING_PARAM_NAME,
            STRING_PARAM_NATIVE,
            BOOLEAN_PARAM_NAME,
            BOOLEAN_PARAM_NATIVE_STRING,
            INT_PARAM_NAME,
            INT_PARAM_NATIVE_STRING,
            DOUBLE_PARAM_NAME,
            DOUBLE_PARAM_NATIVE_STRING,
    };
    private static final Map<String, String> ALL_PARAM_TEST_OVERRIDE = Map.of(
            STRING_PARAM_NAME, STRING_PARAM_TEST_OVERRIDE,
            BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_TEST_OVERRIDE_STRING,
            INT_PARAM_NAME, INT_PARAM_TEST_OVERRIDE_STRING,
            DOUBLE_PARAM_NAME, DOUBLE_PARAM_TEST_OVERRIDE_STRING);
    // clang-format on

    private static final AllCachedFieldTrialParameters ALL_PARAM =
            new AllCachedFieldTrialParameters(FEATURE_B);

    private static final List<CachedFieldTrialParameter> PARAMS_TO_CACHE =
            List.of(STRING_PARAM, BOOLEAN_PARAM, INT_PARAM, DOUBLE_PARAM, STRING2_PARAM, ALL_PARAM);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(ChromeFeatureListJni.TEST_HOOKS, mChromeFeatureListNativesMock);

        CachedFeatureFlags.resetFlagsForTesting();
        CachedFeatureFlags.resetDiskForTesting();

        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(FEATURE_A, STRING_PARAM_NAME, STRING_PARAM_NATIVE);
        testValues.addFieldTrialParamOverride(
                FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_NATIVE_STRING);
        testValues.addFieldTrialParamOverride(FEATURE_A, INT_PARAM_NAME, INT_PARAM_NATIVE_STRING);
        testValues.addFieldTrialParamOverride(
                FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_NATIVE_STRING);
        testValues.addFieldTrialParamOverride(FEATURE_A, STRING2_PARAM_NAME, STRING2_PARAM_NATIVE);
        FeatureList.setTestValues(testValues);

        when(mChromeFeatureListNativesMock.getFlattedFieldTrialParamsForFeature(eq(FEATURE_B)))
                .thenReturn(ALL_PARAMS_NATIVE);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
        CachedFeatureFlags.resetDiskForTesting();
        FeatureList.setTestValues(null);
    }

    @Test
    public void testNativeNotInitializedNotCached_useDefault() {
        assertValuesAreDefault();
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        CachedFeatureFlags.cacheFieldTrialParameters(PARAMS_TO_CACHE);
        assertValuesAreFromNative();
    }

    @Test
    public void testConsistency() {
        assertValuesAreDefault();
        CachedFeatureFlags.cacheFieldTrialParameters(PARAMS_TO_CACHE);

        // Should still return the values previously returned
        assertValuesAreDefault();
    }

    @Test
    public void testNativeNotInitializedPrefsCached_getsFromPrefs() {
        // Cache to disk
        CachedFeatureFlags.cacheFieldTrialParameters(PARAMS_TO_CACHE);

        // Simulate a second run
        CachedFeatureFlags.resetFlagsForTesting();

        // Set different values in native which shouldn't be used
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(FEATURE_A, STRING_PARAM_NAME, STRING_PARAM_BAD);
        testValues.addFieldTrialParamOverride(FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_BAD);
        testValues.addFieldTrialParamOverride(FEATURE_A, INT_PARAM_NAME, INT_PARAM_BAD);
        testValues.addFieldTrialParamOverride(FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_BAD);
        testValues.addFieldTrialParamOverride(FEATURE_A, STRING2_PARAM_NAME, STRING2_PARAM_BAD);
        FeatureList.setTestValues(testValues);

        // In the second run, should get cached values and not the new ones since
        // CachedFeatureFlags#cacheFieldTrialParameters() wasn't called.
        assertValuesAreFromNative();
    }

    @Test
    public void testSetForTesting() {
        STRING_PARAM.setForTesting(STRING_PARAM_TEST_OVERRIDE);
        BOOLEAN_PARAM.setForTesting(BOOLEAN_PARAM_TEST_OVERRIDE);
        INT_PARAM.setForTesting(INT_PARAM_TEST_OVERRIDE);
        DOUBLE_PARAM.setForTesting(DOUBLE_PARAM_TEST_OVERRIDE);
        STRING2_PARAM.setForTesting(STRING2_PARAM_TEST_OVERRIDE);
        AllCachedFieldTrialParameters.setForTesting(FEATURE_B, ALL_PARAM_TEST_OVERRIDE);

        // Should not take priority over the overrides
        CachedFeatureFlags.cacheFieldTrialParameters(PARAMS_TO_CACHE);

        assertEquals(STRING_PARAM_TEST_OVERRIDE, STRING_PARAM.getValue());
        assertEquals(BOOLEAN_PARAM_TEST_OVERRIDE, BOOLEAN_PARAM.getValue());
        assertEquals(INT_PARAM_TEST_OVERRIDE, INT_PARAM.getValue());
        assertEquals(DOUBLE_PARAM_TEST_OVERRIDE, DOUBLE_PARAM.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_TEST_OVERRIDE, STRING2_PARAM.getValue());
        Map<String, String> featureBParams = ALL_PARAM.getParams();
        assertEquals(STRING_PARAM_TEST_OVERRIDE, featureBParams.get(STRING_PARAM_NAME));
        assertEquals(BOOLEAN_PARAM_TEST_OVERRIDE_STRING, featureBParams.get(BOOLEAN_PARAM_NAME));
        assertEquals(INT_PARAM_TEST_OVERRIDE_STRING, featureBParams.get(INT_PARAM_NAME));
        assertEquals(DOUBLE_PARAM_TEST_OVERRIDE_STRING, featureBParams.get(DOUBLE_PARAM_NAME));
    }

    private void assertValuesAreDefault() {
        assertEquals(STRING_PARAM_DEFAULT, STRING_PARAM.getValue());
        assertEquals(BOOLEAN_PARAM_DEFAULT, BOOLEAN_PARAM.getValue());
        assertEquals(INT_PARAM_DEFAULT, INT_PARAM.getValue());
        assertEquals(DOUBLE_PARAM_DEFAULT, DOUBLE_PARAM.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_DEFAULT, STRING2_PARAM.getValue());

        Map<String, String> featureBParams = ALL_PARAM.getParams();
        assertEquals(0, featureBParams.size());
    }

    private void assertValuesAreFromNative() {
        assertEquals(STRING_PARAM_NATIVE, STRING_PARAM.getValue());
        assertEquals(BOOLEAN_PARAM_NATIVE, BOOLEAN_PARAM.getValue());
        assertEquals(INT_PARAM_NATIVE, INT_PARAM.getValue());
        assertEquals(DOUBLE_PARAM_NATIVE, DOUBLE_PARAM.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_DEFAULT, STRING2_PARAM.getValue()); // Special case

        Map<String, String> featureBParams = ALL_PARAM.getParams();
        assertEquals(4, featureBParams.size());
        assertEquals(STRING_PARAM_NATIVE, featureBParams.get(STRING_PARAM_NAME));
        assertEquals(BOOLEAN_PARAM_NATIVE_STRING, featureBParams.get(BOOLEAN_PARAM_NAME));
        assertEquals(INT_PARAM_NATIVE_STRING, featureBParams.get(INT_PARAM_NAME));
        assertEquals(DOUBLE_PARAM_NATIVE_STRING, featureBParams.get(DOUBLE_PARAM_NAME));
    }
}
