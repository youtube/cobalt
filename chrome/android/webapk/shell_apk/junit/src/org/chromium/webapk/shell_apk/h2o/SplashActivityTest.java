// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowActivityManager;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.shell_apk.CustomAndroidOsShadowAsyncTask;
import org.chromium.webapk.shell_apk.HostBrowserUtils;
import org.chromium.webapk.shell_apk.LaunchHostBrowserSelector;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.Arrays;

/** Tests for {@link SplashActivity}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SplashActivityTest.MockLaunchHostBrowserSelector.class,
                CustomAndroidOsShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public final class SplashActivityTest {
    public static final String BROWSER_PACKAGE_NAME = "com.google.android.apps.chrome";

    /** Mock {@link LaunchHostBrowserSelector} which enables calling the callback manually. */
    @Implements(LaunchHostBrowserSelector.class)
    public static class MockLaunchHostBrowserSelector {
        private static LaunchHostBrowserSelector.Callback sCallback;
        private static boolean sDialogNeeded;

        public void __constructor__(Activity parentActivity) {}

        @Implementation
        public void selectHostBrowser(LaunchHostBrowserSelector.Callback callback) {
            if (!sDialogNeeded) {
                callback.onBrowserSelected(BROWSER_PACKAGE_NAME, false /* dialogShown */);
                return;
            }
            sCallback = callback;
        }

        public static void setNeedsToShowDialog(boolean dialogNeeded) {
            sDialogNeeded = dialogNeeded;
        }

        public static void dialogDismissed() {
            assertNotNull(sCallback);
            sCallback.onBrowserSelected(BROWSER_PACKAGE_NAME, true /* dialogShown */);
            sCallback = null;
        }
    }

    private ShadowActivityManager mShadowActivityManager;
    private ShadowApplication mShadowApplication;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        Context appContext = RuntimeEnvironment.application;
        ActivityManager activityManager =
                (ActivityManager) appContext.getSystemService(Context.ACTIVITY_SERVICE);
        mShadowActivityManager = Shadows.shadowOf(activityManager);
        mShadowApplication = ShadowApplication.getInstance();
        mShadowPackageManager = Shadows.shadowOf(appContext.getPackageManager());

        MockLaunchHostBrowserSelector.setNeedsToShowDialog(false);

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.START_URL, "https://pwa.rocks/");
        metadata.putBoolean(WebApkMetaDataKeys.IS_NEW_STYLE_WEBAPK, true);
        WebApkTestHelper.registerWebApkWithMetaData(appContext.getPackageName(), metadata, null);

        // Install browser.
        mShadowPackageManager.addPackage(newPackageInfo(BROWSER_PACKAGE_NAME,
                HostBrowserUtils.MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH));
    }

    /**
     * Test common cases that SplashActivity:
     * - Does not finish itself when the WebAPK is launched from the app list.
     * - Finishes itself when the user backs out of the activity stacked on top.
     */
    @Test
    public void testNormalLaunch() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());

        // ActivityController#visible() attaches the activity to the window.
        splashActivityController.create(null).visible().resume();
        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());

        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.pause().resume();
        assertTrue(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity finishes itself when:
     * - the user backs out of the activity stacked on top
     * AND
     * - the activity is recreated because it was previously killed by the Android OS due to memory
     *   pressure.
     */
    @Test
    public void testWebApkKilledByOomFinishOnBack() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.resume();
        assertNull(mShadowApplication.getNextStartedActivity());
        assertTrue(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity does not finish itself when:
     * - the choose-host-browser dialog is up
     * AND
     * - the activity is recreated because it was previously killed by the Android OS due to memory
     *   pressure.
     */
    @Test
    public void testWebApkKilledByOomHostBrowserNotSelected() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());
        MockLaunchHostBrowserSelector.setNeedsToShowDialog(true);

        splashActivityController.create(new Bundle()).visible();
        // Dialog shown, LaunchHostBrowserSelector callback called after Activity#onResume()
        splashActivityController.resume();
        MockLaunchHostBrowserSelector.dialogDismissed();
        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity does not finish itself when:
     * - the WebAPK is launched from Android Recents on Android O+
     * AND
     * - the activity is recreated because it was previously killed by the Android OS due to memory
     *   pressure.
     * On pre-O, the activity stacked on top of SplashActivity is recreated but SplashActivity isn't
     * when the user taps the WebAPK in Android recents.
     */
    @Test
    public void testWebApkKilledByOomRecreatedViaRecentsAndroidOPlus() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible().resume();
        assertNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity does not finish itself when:
     * - the WebAPK is launched from a deep link.
     * AND
     * - the WebAPK is already running, but SplashActivity is not running because it was killed by
     *   the Android OS due to memory pressure.
     */
    @Test
    public void testDeepLink() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.newIntent(new Intent());
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.resume();

        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity does not finish itself when it receives onActivityResult()
     * prior to onNewIntent().
     */
    @Test
    public void testActivityResultBeforeNewIntent() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.newIntent(new Intent());
        splashActivityController.resume();

        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Sets {@link ActivityManager#getAppTasks()} to have the passed-in top activity.
     */
    private void setAppTaskTopActivity(int taskId, Activity topActivity) {
        ActivityManager.RecentTaskInfo recentTaskInfo = new ActivityManager.RecentTaskInfo();
        recentTaskInfo.id = taskId;
        recentTaskInfo.topActivity = topActivity.getComponentName();
        ActivityManager.AppTask appTask = mock(ActivityManager.AppTask.class);
        when(appTask.getTaskInfo()).thenReturn(recentTaskInfo);

        mShadowActivityManager.setAppTasks(Arrays.asList(appTask));
    }

    private static PackageInfo newPackageInfo(String packageName, int version) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.versionName = version + ".";
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.metaData = new Bundle();
        return packageInfo;
    }
}
