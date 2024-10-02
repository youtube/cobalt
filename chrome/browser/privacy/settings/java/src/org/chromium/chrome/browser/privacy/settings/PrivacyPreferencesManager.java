// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

/**
 * Manages preferences related to privacy, metrics reporting, prerendering, and network prediction.
 */
public interface PrivacyPreferencesManager extends CrashReportingPermissionManager {
    /**
     * Observer for changes in privacy preferences.
     */
    interface Observer {
        void onIsUsageAndCrashReportingPermittedChanged(boolean permitted);
    }

    /**
     * Adds an {@link Observer}.
     */
    void addObserver(Observer observer);

    /**
     * Removes an {@link Observer}.
     */
    void removeObserver(Observer observer);

    /**
     * Sets the usage and crash reporting preference ON or OFF.
     *
     * @param enabled A boolean corresponding whether usage and crash reports uploads are allowed.
     */
    void setUsageAndCrashReporting(boolean enabled);

    /**
     * Update usage and crash preferences based on Android preferences if possible in case they are
     * out of sync.
     */
    void syncUsageAndCrashReportingPrefs();

    /**
     * Sets whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     */
    void setClientInMetricsSample(boolean inSample);

    /**
     * Checks whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     *
     * @returns boolean Whether client is in-sample.
     */
    @Override
    boolean isClientInMetricsSample();

    /**
     * Checks whether uploading of crash dumps is permitted for the available network(s).
     *
     * @return whether uploading crash dumps is permitted.
     */
    @Override
    boolean isNetworkAvailableForCrashUploads();

    @Override
    boolean isUsageAndCrashReportingPermittedByPolicy();

    /**
     * Checks whether uploading of usage metrics and crash dumps is currently permitted, based on
     * user consent only. This doesn't take network condition or experimental state (i.e. disabling
     * upload) into consideration. A crash dump may be retried if this check passes.
     *
     * @return whether the user has consented to reporting usage metrics and crash dumps.
     *
     * Do not use this API because it doesn't abide by the constraint imposed by the native API.
     * (crbug.com/1203437)
     */
    @Override
    boolean isUsageAndCrashReportingPermittedByUser();

    /**
     * Check whether the command line switch is used to force uploading if at all possible. Used by
     * test devices to avoid UI manipulation.
     *
     * @return whether uploading should be enabled if at all possible.
     */
    @Override
    boolean isUploadEnabledForTests();

    /**
     * @return Whether uploading usage metrics is currently permitted.
     */
    boolean isMetricsUploadPermitted();

    /**
     * @return Whether usage and crash reporting pref is enabled.
     */
    boolean isMetricsReportingEnabled();

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    void setMetricsReportingEnabled(boolean enabled);
}
