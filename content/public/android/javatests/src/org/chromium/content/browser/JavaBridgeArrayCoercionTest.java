// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.webkit.JavascriptInterface;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavaBridgeActivityTestRule.Controller;

/**
 * Part of the test suite for the Java Bridge. This class tests that we correctly convert
 * JavaScript arrays to Java arrays when passing them to the methods of injected Java objects.
 *
 * The conversions should follow
 * http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS. Places in
 * which the implementation differs from the spec are marked with
 * LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not
 * break backwards-compatibility. See b/4408210.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeArrayCoercionTest {
    private static final double ASSERTION_DELTA = 0;

    @Rule
    public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private static class TestObject extends Controller {
        private final Object mObjectInstance;
        private final CustomType mCustomTypeInstance;

        private boolean[] mBooleanArray;
        private byte[] mByteArray;
        private char[] mCharArray;
        private short[] mShortArray;
        private int[] mIntArray;
        private long[] mLongArray;
        private float[] mFloatArray;
        private double[] mDoubleArray;
        private String[] mStringArray;
        private Object[] mObjectArray;
        private CustomType[] mCustomTypeArray;

        public TestObject() {
            mObjectInstance = new Object();
            mCustomTypeInstance = new CustomType();
        }

        @JavascriptInterface
        public Object getObjectInstance() {
            return mObjectInstance;
        }
        @JavascriptInterface
        public CustomType getCustomTypeInstance() {
            return mCustomTypeInstance;
        }

        @JavascriptInterface
        public synchronized void setBooleanArray(boolean[] x) {
            mBooleanArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setByteArray(byte[] x) {
            mByteArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setCharArray(char[] x) {
            mCharArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setShortArray(short[] x) {
            mShortArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setIntArray(int[] x) {
            mIntArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setLongArray(long[] x) {
            mLongArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setFloatArray(float[] x) {
            mFloatArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setDoubleArray(double[] x) {
            mDoubleArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setStringArray(String[] x) {
            mStringArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setObjectArray(Object[] x) {
            mObjectArray = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setCustomTypeArray(CustomType[] x) {
            mCustomTypeArray = x;
            notifyResultIsReady();
        }

        public synchronized boolean[] waitForBooleanArray() {
            waitForResult();
            return mBooleanArray;
        }
        public synchronized byte[] waitForByteArray() {
            waitForResult();
            return mByteArray;
        }
        public synchronized char[] waitForCharArray() {
            waitForResult();
            return mCharArray;
        }
        public synchronized short[] waitForShortArray() {
            waitForResult();
            return mShortArray;
        }
        public synchronized int[] waitForIntArray() {
            waitForResult();
            return mIntArray;
        }
        public synchronized long[] waitForLongArray() {
            waitForResult();
            return mLongArray;
        }
        public synchronized float[] waitForFloatArray() {
            waitForResult();
            return mFloatArray;
        }
        public synchronized double[] waitForDoubleArray() {
            waitForResult();
            return mDoubleArray;
        }
        public synchronized String[] waitForStringArray() {
            waitForResult();
            return mStringArray;
        }
        public synchronized Object[] waitForObjectArray() {
            waitForResult();
            return mObjectArray;
        }
        public synchronized CustomType[] waitForCustomTypeArray() {
            waitForResult();
            return mCustomTypeArray;
        }
    }

    // Two custom types used when testing passing objects.
    private static class CustomType {
    }

    @UseMethodParameterBefore(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void setupMojoTest(boolean useMojo) {
        mActivityTestRule.setupMojoTest(useMojo);
    }

    private TestObject mTestObject;

    @Before
    public void setUp() {
        mTestObject = new TestObject();
        mActivityTestRule.injectObjectAndReload(mTestObject, "testObject");
    }

    // Note that all tests use a single element array for simplicity. We test
    // multiple elements elsewhere.

    // Test passing an array of JavaScript numbers in the int32 range to a
    // method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassNumberInt32(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([0]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);
        // LIVECONNECT_COMPLIANCE: Should convert to boolean.
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([42]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray([42]);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray([42]);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([42]);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([42]);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([42]);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([42]);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([42]);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([42]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([42]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([42]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript numbers in the double range to a
    // method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassNumberDouble(boolean useMojo) throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should convert to boolean.
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([42.1]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray([42.1]);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharArray([42.1]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([42.1]);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([42.1]);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([42.1]);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([42.1]);");
        Assert.assertEquals(42.1f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([42.1]);");
        Assert.assertEquals(42.1, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([42.1]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([42.1]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([42.1]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript NaN values to a method which takes a
    // Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassNumberNaN(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([Number.NaN]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray([Number.NaN]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray([Number.NaN]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([Number.NaN]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([Number.NaN]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([Number.NaN]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([Number.NaN]);");
        Assert.assertEquals(Float.NaN, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([Number.NaN]);");
        Assert.assertEquals(Double.NaN, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([Number.NaN]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([Number.NaN]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([Number.NaN]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript infinity values to a method which
    // takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassNumberInfinity(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([Infinity]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray([Infinity]);");
        Assert.assertEquals(-1, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to maximum numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharArray([Infinity]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([Infinity]);");
        Assert.assertEquals(-1, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([Infinity]);");
        Assert.assertEquals(Integer.MAX_VALUE, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([Infinity]);");
        Assert.assertEquals(Long.MAX_VALUE, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([Infinity]);");
        Assert.assertEquals(
                Float.POSITIVE_INFINITY, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([Infinity]);");
        Assert.assertEquals(
                Double.POSITIVE_INFINITY, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([Infinity]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([Infinity]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([Infinity]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript boolean values to a method which
    // takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassBoolean(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([true]);");
        Assert.assertTrue(mTestObject.waitForBooleanArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([false]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setByteArray([true]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setByteArray([false]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value 1.
        mActivityTestRule.executeJavaScript("testObject.setCharArray([true]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setCharArray([false]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setShortArray([true]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setShortArray([false]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setIntArray([true]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setIntArray([false]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        mActivityTestRule.executeJavaScript("testObject.setLongArray([true]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);
        mActivityTestRule.executeJavaScript("testObject.setLongArray([false]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        mActivityTestRule.executeJavaScript("testObject.setFloatArray([true]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);
        mActivityTestRule.executeJavaScript("testObject.setFloatArray([false]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([true]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);
        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([false]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([true]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([true]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([true]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript strings to a method which takes a
    // Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassString(boolean useMojo) throws Throwable {
        // LIVECONNECT_COMPLIANCE: Non-empty string should convert to true.
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([\"+042.10\"]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setByteArray([\"+042.10\"]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should decode and convert to numeric char value.
        mActivityTestRule.executeJavaScript("testObject.setCharArray([\"+042.10\"]);");
        Assert.assertEquals(0, mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setShortArray([\"+042.10\"]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setIntArray([\"+042.10\"]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setLongArray([\"+042.10\"]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setFloatArray([\"+042.10\"]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([\"+042.10\"]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([\"+042.10\"]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray([\"+042.10\"]);");
        Assert.assertEquals("+042.10", mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([\"+042.10\"]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript objects to a method which takes a
    // Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassJavaScriptObject(boolean useMojo) throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([{foo: 42}]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setByteArray([{foo: 42}]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCharArray([{foo: 42}]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setShortArray([{foo: 42}]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setIntArray([{foo: 42}]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setLongArray([{foo: 42}]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setFloatArray([{foo: 42}]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([{foo: 42}]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([{foo: 42}]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        mActivityTestRule.executeJavaScript("testObject.setStringArray([{foo: 42}]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([{foo: 42}]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of Java objects to a method which takes a Java
    // array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassJavaObject(boolean useMojo) throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setBooleanArray([testObject.getObjectInstance()]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setByteArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setCharArray([testObject.getObjectInstance()]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setShortArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setIntArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setLongArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setFloatArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        mActivityTestRule.executeJavaScript(
                "testObject.setDoubleArray([testObject.getObjectInstance()]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        // LIVECONNECT_COMPLIANCE: Should create an array and pass Java object.
        mActivityTestRule.executeJavaScript(
                "testObject.setObjectArray([testObject.getObjectInstance()]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        mActivityTestRule.executeJavaScript(
                "testObject.setStringArray([testObject.getObjectInstance()]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass Java object.
        mActivityTestRule.executeJavaScript(
                "testObject.setCustomTypeArray([testObject.getObjectInstance()]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
        mActivityTestRule.executeJavaScript(
                "testObject.setCustomTypeArray([testObject.getCustomTypeInstance()]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript null values to a method which takes
    // a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassNull(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteArray([null]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray([null]);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([null]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([null]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([null]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([null]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([null]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([null]);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([null]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray([null]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([null]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript undefined values to a method which
    // takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUndefined(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setByteArray([undefined]);");
        Assert.assertEquals(0, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray([undefined]);");
        Assert.assertEquals(0, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray([undefined]);");
        Assert.assertEquals(0, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray([undefined]);");
        Assert.assertEquals(0, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray([undefined]);");
        Assert.assertEquals(0L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray([undefined]);");
        Assert.assertEquals(0.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray([undefined]);");
        Assert.assertEquals(0.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray([undefined]);");
        Assert.assertEquals(false, mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        mActivityTestRule.executeJavaScript("testObject.setObjectArray([undefined]);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray([undefined]);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray([undefined]);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Int8Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassInt8Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(1);");
        mActivityTestRule.executeJavaScript("int8_array = new Int8Array(buffer);");
        mActivityTestRule.executeJavaScript("int8_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(int8_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(int8_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(int8_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(int8_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(int8_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(int8_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(int8_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(int8_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(int8_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(int8_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(int8_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Int8Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testPassInt8ArrayWithNagativeValue(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(1);");
        mActivityTestRule.executeJavaScript("int8_array = new Int8Array(buffer);");
        mActivityTestRule.executeJavaScript("int8_array[0] = -1;");

        mActivityTestRule.executeJavaScript("testObject.setByteArray(int8_array);");
        Assert.assertEquals(-1, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(int8_array);");
        Assert.assertEquals(65535, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(int8_array);");
        Assert.assertEquals(-1, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(int8_array);");
        Assert.assertEquals(-1, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(int8_array);");
        Assert.assertEquals(-1L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(int8_array);");
        Assert.assertEquals(-1.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(int8_array);");
        Assert.assertEquals(-1.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);
    }

    // Test passing a typed Uint8Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUint8Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(1);");
        mActivityTestRule.executeJavaScript("uint8_array = new Uint8Array(buffer);");
        mActivityTestRule.executeJavaScript("uint8_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(uint8_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint8_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint8_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint8_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint8_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint8_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint8_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint8_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(uint8_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(uint8_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(uint8_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testPassUint8ArrayWithMaxValue(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(1);");
        mActivityTestRule.executeJavaScript("uint8_array = new Uint8Array(buffer);");
        mActivityTestRule.executeJavaScript("uint8_array[0] = 255;");

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint8_array);");
        Assert.assertEquals(-1, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint8_array);");
        Assert.assertEquals(255, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint8_array);");
        Assert.assertEquals(255, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint8_array);");
        Assert.assertEquals(255, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint8_array);");
        Assert.assertEquals(255L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint8_array);");
        Assert.assertEquals(255.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint8_array);");
        Assert.assertEquals(255.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);
    }

    // Test passing a typed Int16Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassInt16Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(2);");
        mActivityTestRule.executeJavaScript("int16_array = new Int16Array(buffer);");
        mActivityTestRule.executeJavaScript("int16_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(int16_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(int16_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(int16_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(int16_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(int16_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(int16_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(int16_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(int16_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(int16_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(int16_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(int16_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint16Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUint16Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(2);");
        mActivityTestRule.executeJavaScript("uint16_array = new Uint16Array(buffer);");
        mActivityTestRule.executeJavaScript("uint16_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(uint16_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint16_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint16_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint16_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint16_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint16_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint16_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint16_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(uint16_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(uint16_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(uint16_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint16Array of max values to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUint16ArrayWithMaxValue(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(2);");
        mActivityTestRule.executeJavaScript("uint16_array = new Uint16Array(buffer);");
        mActivityTestRule.executeJavaScript("uint16_array[0] = 65535;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(uint16_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint16_array);");
        Assert.assertEquals(-1, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint16_array);");
        Assert.assertEquals(65535, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint16_array);");
        Assert.assertEquals(-1, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint16_array);");
        Assert.assertEquals(65535, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint16_array);");
        Assert.assertEquals(65535L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint16_array);");
        Assert.assertEquals(65535.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint16_array);");
        Assert.assertEquals(65535.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);
    }

    // Test passing a typed Int32Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassInt32Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(4);");
        mActivityTestRule.executeJavaScript("int32_array = new Int32Array(buffer);");
        mActivityTestRule.executeJavaScript("int32_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(int32_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(int32_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(int32_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(int32_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(int32_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(int32_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(int32_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(int32_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(int32_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(int32_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(int32_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint32Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUint32Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(4);");
        mActivityTestRule.executeJavaScript("uint32_array = new Uint32Array(buffer);");
        mActivityTestRule.executeJavaScript("uint32_array[0] = 42;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(uint32_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint32_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint32_array);");
        Assert.assertEquals(42, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint32_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint32_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint32_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint32_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint32_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(uint32_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(uint32_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(uint32_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint32Array of max values to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassUint32ArrayWithMaxValue(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(4);");
        mActivityTestRule.executeJavaScript("uint32_array = new Uint32Array(buffer);");
        mActivityTestRule.executeJavaScript("uint32_array[0] = 4294967295;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(uint32_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(uint32_array);");
        Assert.assertEquals(-1, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(uint32_array);");
        Assert.assertEquals(65535, mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(uint32_array);");
        Assert.assertEquals(-1, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(uint32_array);");
        Assert.assertEquals(-1, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(uint32_array);");
        Assert.assertEquals(4294967295L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(uint32_array);");
        Assert.assertEquals((Long.valueOf(4294967295L)).floatValue(),
                mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(uint32_array);");
        Assert.assertEquals(4294967295.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);
    }

    // Test passing a typed Float32Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassFloat32Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(4);");
        mActivityTestRule.executeJavaScript("float32_array = new Float32Array(buffer);");
        mActivityTestRule.executeJavaScript("float32_array[0] = 42.0;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(float32_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(float32_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(float32_array);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(float32_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(float32_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(float32_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(float32_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(float32_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(float32_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(float32_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(float32_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Float64Array to a method which takes a Java array.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testPassFloat64Array(boolean useMojo) throws Throwable {
        mActivityTestRule.executeJavaScript("buffer = new ArrayBuffer(8);");
        mActivityTestRule.executeJavaScript("float64_array = new Float64Array(buffer);");
        mActivityTestRule.executeJavaScript("float64_array[0] = 42.0;");

        mActivityTestRule.executeJavaScript("testObject.setBooleanArray(float64_array);");
        Assert.assertFalse(mTestObject.waitForBooleanArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setByteArray(float64_array);");
        Assert.assertEquals(42, mTestObject.waitForByteArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCharArray(float64_array);");
        Assert.assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setShortArray(float64_array);");
        Assert.assertEquals(42, mTestObject.waitForShortArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setIntArray(float64_array);");
        Assert.assertEquals(42, mTestObject.waitForIntArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setLongArray(float64_array);");
        Assert.assertEquals(42L, mTestObject.waitForLongArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setFloatArray(float64_array);");
        Assert.assertEquals(42.0f, mTestObject.waitForFloatArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setDoubleArray(float64_array);");
        Assert.assertEquals(42.0, mTestObject.waitForDoubleArray()[0], ASSERTION_DELTA);

        mActivityTestRule.executeJavaScript("testObject.setObjectArray(float64_array);");
        Assert.assertNull(mTestObject.waitForObjectArray());

        mActivityTestRule.executeJavaScript("testObject.setStringArray(float64_array);");
        Assert.assertNull(mTestObject.waitForStringArray()[0]);

        mActivityTestRule.executeJavaScript("testObject.setCustomTypeArray(float64_array);");
        Assert.assertNull(mTestObject.waitForCustomTypeArray());
    }
}
