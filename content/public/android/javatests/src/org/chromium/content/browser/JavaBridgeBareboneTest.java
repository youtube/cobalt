// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
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
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;

/**
 * Common functionality for testing the Java Bridge.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeBareboneTest {
    @Rule
    public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private TestCallbackHelperContainer mTestCallbackHelperContainer;
    private boolean mUseMojo;

    @UseMethodParameterBefore(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void setupMojoTest(boolean useMojo) {
        mUseMojo = useMojo;
        mActivityTestRule.setupMojoTest(useMojo);
    }

    private void injectDummyObject(final String name) throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(mUseMojo).addPossiblyUnsafeInterface(
                        new Object(), name, null);
            }
        });
    }

    private String evaluateJsSync(String jsCode) throws Exception {
        OnEvaluateJavaScriptResultHelper javascriptHelper = new OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(mActivityTestRule.getWebContents(), jsCode);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    // If inection happens before evaluating any JS code, then the first evaluation
    // triggers the same condition as page reload, which causes Java Bridge to add
    // a JS wrapper.
    // This contradicts the statement of our documentation that injected objects are
    // only available after the next page (re)load, but it is too late to fix that,
    // as existing apps may implicitly rely on this behaviour, something like:
    //
    //   webView.loadUrl(...);
    //   webView.addJavascriptInterface(new Foo(), "foo");
    //   webView.loadUrl("javascript:foo.bar();void(0);");
    //
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testImmediateAddition(boolean useMojo) throws Throwable {
        injectDummyObject("testObject");
        Assert.assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }

    // Now here, we evaluate some JS before injecting the object, and this behaves as
    // expected.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testNoImmediateAdditionAfterJSEvaluation(boolean useMojo) throws Throwable {
        evaluateJsSync("true");
        injectDummyObject("testObject");
        Assert.assertEquals("\"undefined\"", evaluateJsSync("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testImmediateAdditionAfterReload(boolean useMojo) throws Throwable {
        mActivityTestRule.synchronousPageReload();
        injectDummyObject("testObject");
        Assert.assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void testReloadAfterAddition(boolean useMojo) throws Throwable {
        injectDummyObject("testObject");
        mActivityTestRule.synchronousPageReload();
        Assert.assertEquals("\"object\"", evaluateJsSync("typeof testObject"));
    }
}
