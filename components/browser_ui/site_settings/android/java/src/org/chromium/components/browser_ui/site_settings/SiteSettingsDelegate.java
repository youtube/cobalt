// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.Set;

/**
 * An interface implemented by the embedder that allows the Site Settings UI to access
 * embedder-specific logic.
 */
public interface SiteSettingsDelegate {
    /**
     * @return The BrowserContextHandle that should be used to read and update settings.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return the ManagedPreferenceDelegate instance that should be used when rendering
     *         Preferences.
     */
    ManagedPreferenceDelegate getManagedPreferenceDelegate();

    /**
     * Asynchronously looks up the locally cached favicon image for the given URL, generating a
     * fallback if one isn't available.
     *
     * @param faviconUrl The URL of the page to get the favicon for. If a favicon for the full URL
     *     can't be found, the favicon for its host will be used as a fallback.
     * @param callback A callback that will be called with the favicon bitmap, or null if no
     *     favicon could be found or generated.
     */
    void getFaviconImageForURL(GURL faviconUrl, Callback<Drawable> callback);

    /**
     * @return true if the given category type should be shown in the SiteSettings Fragment.
     */
    boolean isCategoryVisible(@SiteSettingsCategory.Type int type);

    /**
     * @return true if Incognito mode is enabled.
     */
    boolean isIncognitoModeEnabled();

    /**
     * @return true if the QuietNotificationPrompts Feature is enabled.
     */
    boolean isQuietNotificationPromptsFeatureEnabled();

    /**
     * @return true if the PrivacySandboxFirstPartySetsUI Feature is enabled.
     */
    boolean isPrivacySandboxFirstPartySetsUIFeatureEnabled();

    /**
     * @return true if the PrivacySandboxSettings4 feature is enabled.
     */
    boolean isPrivacySandboxSettings4Enabled();

    /**
     * @return The id of the notification channel associated with the given origin.
     */
    // TODO(crbug.com/1069895): Remove this once WebLayer supports notifications.
    String getChannelIdForOrigin(String origin);

    /**
     * @return The name of the app the settings are associated with.
     */
    String getAppName();

    /**
     * @return The user visible name of the app that will handle permission delegation for the
     *     origin and content setting type.
     */
    @Nullable
    String getDelegateAppNameForOrigin(Origin origin, @ContentSettingsType int type);

    /**
     * @return The package name of the app that should handle permission delegation for the origin
     *     and content setting type.
     */
    @Nullable
    String getDelegatePackageNameForOrigin(Origin origin, @ContentSettingsType int type);

    /**
     * @return true if Help and Feedback links and menu items should be shown to the user.
     */
    boolean isHelpAndFeedbackEnabled();

    /**
     * Launches a support page relevant to settings UI pages.
     *
     * @see org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher#show
     */
    void launchSettingsHelpAndFeedbackActivity(Activity currentActivity);

    /**
     * Launches a support page related to protected content.
     *
     * @see org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher#show
     */
    void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity);

    /**
     * @return The set of all origins that have a WebAPK or TWA installed.
     */
    Set<String> getOriginsWithInstalledApp();

    /**
     * @return The set of all origins whose notification permissions are delegated to another app.
     */
    Set<String> getAllDelegatedNotificationOrigins();

    /**
     * Displays a snackbar, informing the user about the Privacy Sandbox settings page, when the
     * corresponding flag is enabled.
     */
    void maybeDisplayPrivacySandboxSnackbar();

    /**
     * Dismisses the Privacy Sandbox snackbar, if active.
     */
    void dismissPrivacySandboxSnackbar();

    /***
     * @return true if First Party Sets data access is enabled.
     */
    boolean isFirstPartySetsDataAccessEnabled();

    /***
     * @return true if First Party Sets data access is managed.
     */
    boolean isFirstPartySetsDataAccessManaged();

    /***
     * @param origin to check.
     * @return true if the origin is part of the managed FirstPartySet.
     */
    boolean isPartOfManagedFirstPartySet(String origin);

    /***
     * @return Enables/disables First Party Sets data access.
     */
    void setFirstPartySetsDataAccessEnabled(boolean enabled);

    /**
     * Gets the First Party Sets owner hostname given a FPS member origin.
     * @param memberOrigin FPS member origin.
     * @return A string containing the owner hostname, null if it doesn't exist.
     */
    String getFirstPartySetOwner(String memberOrigin);

    /**
     * Returns whether the current implementation of the delegate is able to launch the Clear
     * Browsing Data dialog in Settings.
     */
    boolean canLaunchClearBrowsingDataDialog();

    /**
     * Launches the Clear Browsing Data dialog in Settings, if that is possible.
     */
    void launchClearBrowsingDataDialog(Activity currentActivity);

    /**
     * Called when the desktop site settings page is opened.
     */
    void notifyRequestDesktopSiteSettingsPageOpened();

    /**
     * Called when the view this delegate is assigned to gets destroyed.
     */
    void onDestroyView();
}
