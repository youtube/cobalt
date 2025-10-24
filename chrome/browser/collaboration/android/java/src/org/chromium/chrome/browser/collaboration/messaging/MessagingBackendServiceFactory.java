// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration.messaging;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.collaboration.messaging.MessagingBackendService;

/** This factory class creates a MessagingBackendService for a give {@link Profile}. */
@JNINamespace("collaboration::messaging::android")
@NullMarked
public final class MessagingBackendServiceFactory {
    private static @Nullable MessagingBackendService sMessagingBackendServiceForTesting;

    /**
     * A factory method to create or retrieve a {@link MessagingBackendService} object for a given
     * profile.
     *
     * @return The {@link MessageBackendService} for the given profile.
     */
    public static MessagingBackendService getForProfile(Profile profile) {
        if (sMessagingBackendServiceForTesting != null) {
            return sMessagingBackendServiceForTesting;
        }

        assert !profile.isOffTheRecord();

        return MessagingBackendServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Sets a {@link MessageBackendService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@link MessagingBackendService} to use for testing, or null if the
     *     real service should be used.
     */
    public static void setForTesting(@Nullable MessagingBackendService testService) {
        sMessagingBackendServiceForTesting = testService;
        ResettersForTesting.register(() -> sMessagingBackendServiceForTesting = null);
    }

    private MessagingBackendServiceFactory() {}

    @NativeMethods
    interface Natives {
        MessagingBackendService getForProfile(@JniType("Profile*") Profile profile);
    }
}
