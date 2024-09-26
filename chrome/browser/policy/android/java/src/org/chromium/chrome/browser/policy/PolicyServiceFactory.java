// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.policy.PolicyService;

/**
 * Get the PolicyService instance. Note that the associated C++ instance won't
 * notify its deletion. It's caller's responsibility to make sure the instance
 * is still valid.
 */
@JNINamespace("policy::android")
public class PolicyServiceFactory {
    private static PolicyService sPolicyServiceForTest;

    /**
     * Returns the PolicyService instance that contains browser policies.
     * The associated C++ instance is deleted during shutdown.
     */
    public static PolicyService getGlobalPolicyService() {
        return sPolicyServiceForTest == null
                ? PolicyServiceFactoryJni.get().getGlobalPolicyService()
                : sPolicyServiceForTest;
    }

    /**
     * Returns the PolicyService instance that contains |profile|'s policies.
     * The associated C++ instance is deleted during shutdown or {@link Profile}
     * deletion.
     */
    public static PolicyService getProfilePolicyService(Profile profile) {
        return sPolicyServiceForTest == null
                ? PolicyServiceFactoryJni.get().getProfilePolicyService(profile)
                : sPolicyServiceForTest;
    }

    /**
     * @param policyService Mock {@link PolicyService} for testing.
     */
    @VisibleForTesting
    public static void setPolicyServiceForTest(PolicyService policyService) {
        sPolicyServiceForTest = policyService;
    }

    @NativeMethods
    public interface Natives {
        PolicyService getGlobalPolicyService();
        PolicyService getProfilePolicyService(Profile profile);
    }
}
