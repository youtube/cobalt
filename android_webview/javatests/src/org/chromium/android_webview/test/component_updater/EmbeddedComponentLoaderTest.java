// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.component_updater;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.util.EmbeddedComponentLoaderFactory;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.component_updater.EmbeddedComponentLoader;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Test for {@link EmbeddedComponentLoader}. It's an integeration-like test where it uses mock
 * native loaders and connect to {@link MockComponentProviderService}.
 *
 * Some test assertion are made in test/browser/embedded_component_loader_test_helper.cc
 */
@RunWith(AwJUnit4ClassRunner.class)
@JNINamespace("component_updater")
public class EmbeddedComponentLoaderTest {
    private static CallbackHelper sOnComponentLoadedHelper = new CallbackHelper();
    private static CallbackHelper sOnComponentLoadFailedHelper = new CallbackHelper();
    private static List<String> sNativeErrors;

    private static final String TEST_COMPONENT_ID = "jebgalgnebhfojomionfpkfelancnnkf";
    private static final String MANIFEST_JSON_STRING = "{"
            + "\n\"manifest_version\": 2,"
            + "\n\"name\": \"jebgalgnebhfojomionfpkfelancnnkf\","
            + "\n\"version\": \"123.456.789\""
            + "\n}";

    // Use AwActivityTestRule to start a browser process and init native library.
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Before
    public void setUp() throws Exception {
        sNativeErrors = new ArrayList<>();
        File testDirectory = getTestDirectory();
        Assert.assertTrue(testDirectory.isDirectory() || testDirectory.mkdirs());
    }

    @After
    public void tearDown() {
        Assert.assertTrue("Failed to cleanup temporary test files",
                FileUtils.recursivelyDeleteFile(getTestDirectory(), null));
        if (!sNativeErrors.isEmpty()) {
            StringBuilder builder = new StringBuilder();
            builder.append("(");
            builder.append(sNativeErrors.size());
            builder.append(") Native errors occured:");
            for (String error : sNativeErrors) {
                builder.append("\n\n");
                builder.append("Native Error: ");
                builder.append(error);
            }
            Assert.fail(builder.toString());
        }
    }

    @Test
    @MediumTest
    public void testLoadComponentsFromMockComponentsProviderService() throws Exception {
        loadComponents(MockComponentsProviderService.class);
    }

    @Test
    @MediumTest
    public void testLoadComponents() throws Exception {
        loadComponents(ComponentsProviderService.class);
    }

    private void loadComponents(Class serviceClass) throws Exception {
        int onComponentLoadedCallCount = sOnComponentLoadedHelper.getCallCount();
        int onComponentLoadFailedCallCount = sOnComponentLoadFailedHelper.getCallCount();

        File testDirectory = getTestDirectory();
        File file = new File(testDirectory, "file.test");
        Assert.assertTrue(file.exists() || file.createNewFile());
        File manifestFile = new File(testDirectory, "manifest.json");
        FileUtils.copyStreamToFile(
                new ByteArrayInputStream(MANIFEST_JSON_STRING.getBytes()), manifestFile);

        Intent intent = new Intent(ContextUtils.getApplicationContext(), serviceClass);
        intent.putExtra(TEST_COMPONENT_ID,
                new String[] {file.getAbsolutePath(), manifestFile.getAbsolutePath()});

        mActivityTestRule.runOnUiThread(() -> {
            EmbeddedComponentLoader mLoader =
                    EmbeddedComponentLoaderFactory.makeEmbeddedComponentLoader();
            mLoader.connect(intent);
        });

        // Should be called once for AvailableComponentLoaderPolicy.
        sOnComponentLoadedHelper.waitForCallback(
                "Timed out waiting for onComponentLoaded() to be called",
                onComponentLoadedCallCount, 1, AwActivityTestRule.WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        // Should be called once for UnavailableComponentLoaderPolicy.
        sOnComponentLoadFailedHelper.waitForCallback(
                "Timed out waiting for onComponentLoadFailed() to be called",
                onComponentLoadFailedCallCount, 1, AwActivityTestRule.WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    @CalledByNative
    private static void onComponentLoaded() {
        sOnComponentLoadedHelper.notifyCalled();
    }

    @CalledByNative
    private static void onComponentLoadFailed() {
        sOnComponentLoadFailedHelper.notifyCalled();
    }

    @CalledByNative
    private static void fail(String error) {
        sNativeErrors.add(error);
    }

    private static File getTestDirectory() {
        return new File(ComponentsProviderPathUtil.getComponentsServingDirectoryPath(),
                TEST_COMPONENT_ID + "/3_123.456.789");
    }
}
