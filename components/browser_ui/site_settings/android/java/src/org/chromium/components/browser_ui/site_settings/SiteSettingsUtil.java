// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.text.format.Formatter;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;

/**
 * Util class for site settings UI.
 */
public class SiteSettingsUtil {
    // Defining the order for content settings based on http://crbug.com/610358
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final int[] SETTINGS_ORDER = {
            ContentSettingsType.COOKIES,
            ContentSettingsType.GEOLOCATION,
            ContentSettingsType.MEDIASTREAM_CAMERA,
            ContentSettingsType.MEDIASTREAM_MIC,
            ContentSettingsType.NOTIFICATIONS,
            ContentSettingsType.JAVASCRIPT,
            ContentSettingsType.POPUPS,
            ContentSettingsType.ADS,
            ContentSettingsType.BACKGROUND_SYNC,
            ContentSettingsType.AUTOMATIC_DOWNLOADS,
            ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
            ContentSettingsType.SOUND,
            ContentSettingsType.MIDI_SYSEX,
            ContentSettingsType.CLIPBOARD_READ_WRITE,
            ContentSettingsType.NFC,
            ContentSettingsType.BLUETOOTH_SCANNING,
            ContentSettingsType.VR,
            ContentSettingsType.AR,
            ContentSettingsType.IDLE_DETECTION,
            ContentSettingsType.FEDERATED_IDENTITY_API,
            ContentSettingsType.SENSORS,
            ContentSettingsType.AUTO_DARK_WEB_CONTENT,
            ContentSettingsType.REQUEST_DESKTOP_SITE,
    };

    static final int[] CHOOSER_PERMISSIONS = {
            ContentSettingsType.USB_CHOOSER_DATA,
            // Bluetooth is only shown when WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND is enabled.
            ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
    };

    /**
     * @param types A list of ContentSettingsTypes
     * @return The highest priority permission that is available in SiteSettings. Returns DEFAULT
     *         when called with empty list or only with entries not represented in this UI.
     */
    public static @ContentSettingsType int getHighestPriorityPermission(
            @ContentSettingsType @NonNull int[] types) {
        for (@ContentSettingsType int setting : SETTINGS_ORDER) {
            for (@ContentSettingsType int type : types) {
                if (setting == type) {
                    return type;
                }
            }
        }
        for (@ContentSettingsType int setting : CHOOSER_PERMISSIONS) {
            for (@ContentSettingsType int type : types) {
                if (type == ContentSettingsType.BLUETOOTH_CHOOSER_DATA
                        && !ContentFeatureList.isEnabled(
                                ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
                    continue;
                }
                if (setting == type) {
                    return type;
                }
            }
        }
        return ContentSettingsType.DEFAULT;
    }

    /**
     * @return whether the flag for the improved UI for "All sites" and "Site settings" is enabled.
     */
    public static boolean isSiteDataImprovementEnabled() {
        return SiteSettingsFeatureList.isEnabled(SiteSettingsFeatureList.SITE_DATA_IMPROVEMENTS);
    }

    /**
     * @param context A {@link Context} object to pull strings out of.
     * @param storage The amount of storage (in bytes) used by the entry.
     * @param cookies The number of cookies associated with the entry.
     * @return A string to display in the UI to show and clear storage and cookies.
     */
    public static String generateStorageUsageText(Context context, long storage, int cookies) {
        String result = "";
        if (storage > 0) {
            result = String.format(context.getString(R.string.origin_settings_storage_usage_brief),
                    Formatter.formatShortFileSize(context, storage));
        }
        if (cookies > 0) {
            String cookie_str = context.getResources().getQuantityString(
                    R.plurals.cookies_count, cookies, cookies);
            result = result.isEmpty()
                    ? cookie_str
                    : String.format(context.getString(R.string.summary_with_one_bullet), result,
                            cookie_str);
        }
        return result;
    }
}
