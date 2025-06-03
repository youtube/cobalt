// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runners.model.InitializationError;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.gms.ChromiumPlayServicesAvailability;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * A custom runner for //chrome JUnit4 tests.
 */
public class ChromeJUnit4ClassRunner extends ContentJUnit4ClassRunner {
    /**
     * Create a ChromeJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public ChromeJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(super.getSkipChecks(),
                new ChromeRestrictionSkipCheck(ApplicationProvider.getApplicationContext()));
    }

    @Override
    protected List<TestHook> getPreTestHooks() {
        return addToList(super.getPreTestHooks(), Policies.getRegistrationHook());
    }

    @Override
    protected List<TestRule> getDefaultTestRules() {
        List<TestRule> rules = super.getDefaultTestRules();
        Class<?> clazz = getTestClass().getJavaClass();
        if (!clazz.isAnnotationPresent(Batch.class)
                || !clazz.getAnnotation(Batch.class).value().equals(Batch.UNIT_TESTS)) {
            rules = addToList(rules, new Features.InstrumentationProcessor());
        }

        return addToList(rules, new DisableAnimationsTestRule());
    }

    private static class ChromeRestrictionSkipCheck extends RestrictionSkipCheck {
        public ChromeRestrictionSkipCheck(Context targetContext) {
            super(targetContext);
        }

        private Class getDaydreamApiClass() {
            Class daydreamApiClass;
            try {
                daydreamApiClass = Class.forName("com.google.vr.ndk.base.DaydreamApi");
            } catch (ClassNotFoundException e) {
                daydreamApiClass = null;
            }
            return daydreamApiClass;
        }

        @SuppressWarnings("unchecked")
        private boolean isDaydreamReady() {
            // We normally check things like this through VrShellDelegate. However, with the
            // introduction of Dynamic Feature Modules (DFMs), we have tests that expect the VR
            // DFM to not be loaded. Using the normal approach (VrModuleProvider.getDelegate())
            // causes the DFM to be loaded, which we don't want done before the test starts. So,
            // access the Daydream API directly. VR is likely, but not guaranteed, to be compiled
            // into the test binary, so use reflection.
            Class daydreamApiClass = getDaydreamApiClass();
            if (daydreamApiClass == null) {
                return false;
            }
            try {
                Method isDaydreamMethod =
                        daydreamApiClass.getMethod("isDaydreamReadyPlatform", Context.class);
                Boolean isDaydream = (Boolean) isDaydreamMethod.invoke(
                        null, ContextUtils.getApplicationContext());
                return isDaydream;
            } catch (NoSuchMethodException | SecurityException | IllegalAccessException
                    | IllegalArgumentException | InvocationTargetException e) {
                return false;
            }
        }

        @SuppressWarnings("unchecked")
        private boolean isDaydreamViewPaired() {
            if (!isDaydreamReady()) {
                return false;
            }

            Class daydreamApiClass = getDaydreamApiClass();
            if (daydreamApiClass == null) {
                return false;
            }

            try {
                Method createMethod = daydreamApiClass.getMethod("create", Context.class);
                // We need to ensure that the DaydreamApi instance is created on the main thread.
                Object daydreamApiInstance = TestThreadUtils.runOnUiThreadBlocking(() -> {
                    return createMethod.invoke(null, ContextUtils.getApplicationContext());
                });
                if (daydreamApiInstance == null) {
                    return false;
                }

                Method currentViewerMethod = daydreamApiClass.getMethod("getCurrentViewerType");
                // Getting the current viewer type may result in a disk write in VrCore, so allow
                // that to prevent StrictMode errors.
                Integer viewerType;
                try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                    viewerType = (Integer) currentViewerMethod.invoke(daydreamApiInstance);
                }
                Method closeMethod = daydreamApiClass.getMethod("close");
                closeMethod.invoke(daydreamApiInstance);
                // 1 is the viewer type constant for Daydream headsets. We could use reflection to
                // check against com.google.vr.ndk.base.GvrApi.ViewerType.DAYDREAM, but the
                // constants have never changed, so check this way to be simpler.
                return viewerType == 1;
            } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException
                    | ExecutionException e) {
                return false;
            }
        }

        private boolean isVrDonEnabled() {
            // We can't directly check whether the VR DON flow is enabled since we don't have
            // permission to read the VrCore settings file. Instead, pass a flag.
            return CommandLine.getInstance().hasSwitch("vr-don-enabled");
        }

        @Override
        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(
                        restriction, ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
                    && (!ChromiumPlayServicesAvailability.isGooglePlayServicesAvailable(
                            getTargetContext()))) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_OFFICIAL_BUILD)
                    && (!VersionInfo.isOfficialBuild())) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM)
                    || TextUtils.equals(restriction,
                               ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)) {
                boolean isDaydream = isDaydreamReady();
                if (TextUtils.equals(
                            restriction, ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM)
                        && !isDaydream) {
                    return true;
                } else if (TextUtils.equals(restriction,
                                   ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
                        && isDaydream) {
                    return true;
                }
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM)
                    || TextUtils.equals(restriction,
                               ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)) {
                boolean daydreamViewPaired = isDaydreamViewPaired();
                if (TextUtils.equals(
                            restriction, ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM)
                        && !daydreamViewPaired) {
                    return true;
                } else if (TextUtils.equals(restriction,
                                   ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
                        && daydreamViewPaired) {
                    return true;
                }
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_VR_DON_ENABLED)) {
                return !isVrDonEnabled();
            }
            return false;
        }
    }
}
