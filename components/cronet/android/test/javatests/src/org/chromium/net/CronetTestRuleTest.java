// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunJavaCronet;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.JavaCronetEngine;

/**
 * Tests features of CronetTestRule.
 */
@RunWith(AndroidJUnit4.class)
public class CronetTestRuleTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();
    @Rule
    public final TestName mTestName = new TestName();

    private CronetTestFramework mTestFramework;
    /**
     * For any test whose name contains "MustRun", it's enforced that the test must run and set
     * {@code mTestWasRun} to {@code true}.
     */
    private boolean mTestWasRun;

    @Before
    public void setUp() throws Exception {
        mTestWasRun = false;
        mTestFramework = mTestRule.startCronetTestFramework();
    }

    @After
    public void tearDown() throws Exception {
        if (mTestName.getMethodName().contains("MustRun") && !mTestWasRun) {
            fail(mTestName.getMethodName() + " should have run but didn't.");
        }
    }

    @Test
    @SmallTest
    @RequiresMinApi(999999999)
    public void testRequiresMinApiDisable() {
        fail("RequiresMinApi failed to disable.");
    }

    @Test
    @SmallTest
    @RequiresMinApi(-999999999)
    public void testRequiresMinApiMustRun() {
        mTestWasRun = true;
    }

    @Test
    @SmallTest
    public void testRunBothImplsMustRun() {
        if (mTestRule.testingJavaImpl()) {
            assertFalse(mTestWasRun);
            mTestWasRun = true;
            assertEquals(mTestFramework.mCronetEngine.getClass(), JavaCronetEngine.class);
        } else {
            assertFalse(mTestWasRun);
            mTestWasRun = true;
            assertEquals(mTestFramework.mCronetEngine.getClass(), CronetUrlRequestContext.class);
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testRunOnlyNativeMustRun() {
        assertFalse(mTestRule.testingJavaImpl());
        assertFalse(mTestWasRun);
        mTestWasRun = true;
        assertEquals(mTestFramework.mCronetEngine.getClass(), CronetUrlRequestContext.class);
    }

    @Test
    @SmallTest
    @OnlyRunJavaCronet
    public void testRunOnlyJavaMustRun() {
        assertTrue(mTestRule.testingJavaImpl());
        assertFalse(mTestWasRun);
        mTestWasRun = true;
        assertEquals(mTestFramework.mCronetEngine.getClass(), JavaCronetEngine.class);
    }
}
