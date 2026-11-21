// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.AwPerformanceManagerTestUtilJni;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/** Instrumentation tests verifying Performance Manager end-to-end integration. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwPerformanceManagerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwPerformanceManagerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNavigationCreatesPerformanceManagerNodes() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String workerUrl =
                    webServer.setResponse("/worker.js", "console.log('worker');", null);
            final String indexUrl =
                    webServer.setResponse(
                            "/index.html",
                            "<script>new Worker('" + workerUrl + "');</script>",
                            null);

            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwTestContainerView testView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
            AwContents awContents = testView.getAwContents();
            AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), indexUrl);

            CriteriaHelper.pollUiThread(
                    () ->
                            AwPerformanceManagerTestUtilJni.get()
                                    .verifyGraphNodesExist(
                                            awContents.getWebContents(), indexUrl, workerUrl),
                    "Performance Manager graph nodes (including worker node) were not created.");
        } finally {
            webServer.shutdown();
        }
    }
}
