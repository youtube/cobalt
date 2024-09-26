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
 * Part of the test suite for the Java Bridge. This test tests the
 * use of fields.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeFieldsTest {
    @Rule
    public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private static class TestObject extends Controller {
        private String mStringValue;

        @JavascriptInterface
        // These methods are used to control the test.
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }

        public boolean booleanField = true;
        public byte byteField = 42;
        public char charField = '\u002A';
        public short shortField = 42;
        public int intField = 42;
        public long longField = 42L;
        public float floatField = 42.0f;
        public double doubleField = 42.0;
        public String stringField = "foo";
        public Object objectField = new Object();
        public CustomType customTypeField = new CustomType();
    }

    // A custom type used when testing passing objects.
    private static class CustomType {
    }

    @UseMethodParameterBefore(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void setupMojoTest(boolean useMojo) {
        mActivityTestRule.setupMojoTest(useMojo);
    }

    TestObject mTestObject;

    @Before
    public void setUp() {
        mTestObject = new TestObject();
        mActivityTestRule.injectObjectAndReload(mTestObject, "testObject");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("testObject.setStringValue(" + script + ");");
        return mTestObject.waitForStringValue();
    }

    // The Java bridge does not provide access to fields.
    // FIXME: Consider providing support for this. See See b/4408210.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testFieldTypes(boolean useMojo) throws Throwable {
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.booleanField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.byteField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.charField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.shortField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.intField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.longField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.floatField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.doubleField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.objectField"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.stringField"));
        Assert.assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.customTypeField"));
    }
}
