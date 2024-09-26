// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.FeatureList;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.device.DeviceFeatureList;

/**
 * A class with utility functions that get the appropriate string and icon resources for the
 * Android UI that allows managing content settings.
 */
// The Linter suggests using SparseArray<ResourceItem> instead of a HashMap
// because our key is an int but we're changing the key to a string soon so
// suppress the lint warning for now.
@SuppressLint("UseSparseArrays")
public class ContentSettingsResources {
    /**
     * An inner class contains all the resources for a ContentSettingsType
     */
    private static class ResourceItem {
        private final int mIcon;
        private final int mTitle;
        private final @ContentSettingValues @Nullable Integer mDefaultEnabledValue;
        private final @ContentSettingValues @Nullable Integer mDefaultDisabledValue;
        private final int mEnabledSummary;
        private final int mDisabledSummary;

        ResourceItem(int icon, int title,
                @ContentSettingValues @Nullable Integer defaultEnabledValue,
                @ContentSettingValues @Nullable Integer defaultDisabledValue, int enabledSummary,
                int disabledSummary) {
            mIcon = icon;
            mTitle = title;
            mDefaultEnabledValue = defaultEnabledValue;
            mDefaultDisabledValue = defaultDisabledValue;
            mEnabledSummary = enabledSummary;
            mDisabledSummary = disabledSummary;
        }

        private int getIcon() {
            return mIcon;
        }

        private int getTitle() {
            return mTitle;
        }

        private @ContentSettingValues @Nullable Integer getDefaultEnabledValue() {
            return mDefaultEnabledValue;
        }

        private @ContentSettingValues @Nullable Integer getDefaultDisabledValue() {
            return mDefaultDisabledValue;
        }

        private int getEnabledSummary() {
            return mEnabledSummary == 0 ? getCategorySummary(mDefaultEnabledValue)
                                        : mEnabledSummary;
        }

        private int getDisabledSummary() {
            return mDisabledSummary == 0 ? getCategorySummary(mDefaultDisabledValue)
                                         : mDisabledSummary;
        }
    }

    /**
     * Returns the ResourceItem for a ContentSettingsType.
     * @param delegate A site settings delegate to check feature states. Only needs to be passed
     *                 to access Cookie icon, title or summary.
     */
    private static ResourceItem getResourceItem(int contentType, SiteSettingsDelegate delegate) {
        switch (contentType) {
            case ContentSettingsType.ADS:
                if (delegate == null) {
                    return new ResourceItem(
                            0, 0, ContentSettingValues.ALLOW, ContentSettingValues.BLOCK, 0, 0);
                }
                return new ResourceItem(R.drawable.web_asset,
                        delegate.isPrivacySandboxSettings4Enabled()
                                ? R.string.site_settings_page_intrusive_ads_label
                                : R.string.ads_permission_title,
                        ContentSettingValues.ALLOW, ContentSettingValues.BLOCK,
                        delegate.isPrivacySandboxSettings4Enabled()
                                ? R.string.site_settings_page_intrusive_allowed_sub_label
                                : 0,
                        delegate.isPrivacySandboxSettings4Enabled()
                                ? R.string.site_settings_page_intrusive_blocked_sub_label
                                : R.string.website_settings_category_ads_blocked);

            case ContentSettingsType.ANTI_ABUSE:
                return new ResourceItem(R.drawable.ic_account_attention,
                        R.string.anti_abuse_permission_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK, R.string.anti_abuse_description,
                        R.string.anti_abuse_description);

            case ContentSettingsType.AR:
                return new ResourceItem(R.drawable.gm_filled_cardboard_24,
                        R.string.ar_permission_title, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_ar_ask,
                        R.string.website_settings_category_ar_blocked);

            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return new ResourceItem(R.drawable.infobar_downloading,
                        R.string.automatic_downloads_permission_title, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_ask, 0);

            case ContentSettingsType.AUTO_DARK_WEB_CONTENT:
                return new ResourceItem(R.drawable.ic_brightness_medium_24dp,
                        R.string.auto_dark_web_content_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_auto_dark_allowed,
                        R.string.website_settings_category_auto_dark_blocked);

            case ContentSettingsType.BACKGROUND_SYNC:
                return new ResourceItem(R.drawable.permission_background_sync,
                        R.string.background_sync_permission_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_allowed_recommended, 0);

            case ContentSettingsType.BLUETOOTH_CHOOSER_DATA:
                return new ResourceItem(R.drawable.settings_bluetooth, 0, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, 0, 0);

            case ContentSettingsType.BLUETOOTH_GUARD:
                return new ResourceItem(R.drawable.settings_bluetooth,
                        R.string.website_settings_bluetooth, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_bluetooth_ask,
                        R.string.website_settings_category_bluetooth_blocked);

            case ContentSettingsType.BLUETOOTH_SCANNING:
                return new ResourceItem(R.drawable.gm_filled_bluetooth_searching_24,

                        R.string.website_settings_bluetooth_scanning, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_bluetooth_scanning_ask, 0);

            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                return new ResourceItem(R.drawable.gm_filled_content_paste_24,
                        R.string.clipboard_permission_title, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_clipboard_ask,
                        R.string.website_settings_category_clipboard_blocked);

            case ContentSettingsType.COOKIES:
                if (delegate == null) {
                    return new ResourceItem(
                            0, 0, ContentSettingValues.ALLOW, ContentSettingValues.BLOCK, 0, 0);
                }
                return new ResourceItem(delegate.isPrivacySandboxSettings4Enabled()
                                ? R.drawable.gm_database_24
                                : R.drawable.permission_cookie,
                        delegate.isPrivacySandboxSettings4Enabled() ? R.string.site_data_page_title
                                                                    : R.string.cookies_title,
                        ContentSettingValues.ALLOW, ContentSettingValues.BLOCK,
                        delegate.isPrivacySandboxSettings4Enabled()
                                ? R.string.website_settings_site_data_page_toggle_sub_label_allow
                                : R.string.website_settings_category_cookie_allowed,
                        delegate.isPrivacySandboxSettings4Enabled()
                                ? R.string.website_settings_site_data_page_toggle_sub_label_block
                                : 0);

            case ContentSettingsType.REQUEST_DESKTOP_SITE:
                return new ResourceItem(R.drawable.ic_desktop_windows, R.string.desktop_site_title,
                        ContentSettingValues.ALLOW, ContentSettingValues.BLOCK,
                        R.string.website_settings_category_desktop_site_allowed,
                        R.string.website_settings_category_desktop_site_blocked);

            case ContentSettingsType.FEDERATED_IDENTITY_API:
                return new ResourceItem(R.drawable.ic_account_circle_24dp,
                        R.string.website_settings_federated_identity, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_federated_identity_allowed,
                        R.string.website_settings_category_federated_identity_blocked);

            case ContentSettingsType.GEOLOCATION:
                return new ResourceItem(R.drawable.gm_filled_location_on_24,

                        R.string.website_settings_device_location, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_location_ask,
                        0);

            case ContentSettingsType.IDLE_DETECTION:
                return new ResourceItem(R.drawable.gm_filled_devices_24,
                        R.string.website_settings_idle_detection, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_idle_detection_ask,
                        R.string.website_settings_category_idle_detection_blocked);

            case ContentSettingsType.JAVASCRIPT:
                return new ResourceItem(R.drawable.permission_javascript,
                        R.string.javascript_permission_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_javascript_allowed, 0);

            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return new ResourceItem(R.drawable.gm_filled_videocam_24,
                        R.string.website_settings_use_camera, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_camera_ask,
                        0);

            case ContentSettingsType.MEDIASTREAM_MIC:
                return new ResourceItem(R.drawable.gm_filled_mic_24,
                        R.string.website_settings_use_mic, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_mic_ask, 0);

            case ContentSettingsType.MIDI_SYSEX:
                return new ResourceItem(R.drawable.gm_filled_piano_24,
                        R.string.midi_sysex_permission_title, null, null, 0, 0);

            case ContentSettingsType.NFC:
                return new ResourceItem(R.drawable.gm_filled_nfc_24, R.string.nfc_permission_title,
                        ContentSettingValues.ASK, ContentSettingValues.BLOCK,
                        R.string.website_settings_category_nfc_ask,
                        R.string.website_settings_category_nfc_blocked);

            case ContentSettingsType.NOTIFICATIONS:
                return new ResourceItem(R.drawable.gm_filled_notifications_24,
                        R.string.push_notifications_permission_title, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_notifications_ask, 0);

            case ContentSettingsType.POPUPS:
                return new ResourceItem(R.drawable.permission_popups,
                        R.string.popup_permission_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK, 0,
                        R.string.website_settings_category_popups_redirects_blocked);

            // PROTECTED_MEDIA_IDENTIFIER uses 3-state preference so some values are not used.
            // If 3-state becomes more common we should update localMaps to support it better.
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return new ResourceItem(R.drawable.permission_protected_media,
                        R.string.protected_content, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, 0, 0);

            case ContentSettingsType.SENSORS:
                int sensorsPermissionTitle = R.string.motion_sensors_permission_title;
                int sensorsAllowedDescription =
                        R.string.website_settings_category_motion_sensors_allowed;
                int sensorsBlockedDescription =
                        R.string.website_settings_category_motion_sensors_blocked;
                try {
                    if (FeatureList.isNativeInitialized()
                            && DeviceFeatureList.isEnabled(
                                    DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                        sensorsPermissionTitle = R.string.sensors_permission_title;
                        sensorsAllowedDescription =
                                R.string.website_settings_category_sensors_allowed;
                        sensorsBlockedDescription =
                                R.string.website_settings_category_sensors_blocked;
                    }
                } catch (IllegalArgumentException e) {
                    // We can hit this in tests that use the @Features annotation, as it calls
                    // FeatureList.setTestFeatures() with a map that should not need to contain
                    // DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES.
                }
                return new ResourceItem(R.drawable.settings_sensors, sensorsPermissionTitle,
                        ContentSettingValues.ALLOW, ContentSettingValues.BLOCK,
                        sensorsAllowedDescription, sensorsBlockedDescription);

            case ContentSettingsType.SOUND:
                return new ResourceItem(R.drawable.ic_volume_up_grey600_24dp,
                        R.string.sound_permission_title, ContentSettingValues.ALLOW,
                        ContentSettingValues.BLOCK,
                        R.string.website_settings_category_sound_allowed,
                        R.string.website_settings_category_sound_blocked);

            case ContentSettingsType.USB_CHOOSER_DATA:
                return new ResourceItem(R.drawable.gm_filled_usb_24, 0, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, 0, 0);

            case ContentSettingsType.USB_GUARD:
                return new ResourceItem(R.drawable.gm_filled_usb_24, R.string.website_settings_usb,
                        ContentSettingValues.ASK, ContentSettingValues.BLOCK,
                        R.string.website_settings_category_usb_ask,
                        R.string.website_settings_category_usb_blocked);

            case ContentSettingsType.VR:
                return new ResourceItem(R.drawable.gm_filled_cardboard_24,
                        R.string.vr_permission_title, ContentSettingValues.ASK,
                        ContentSettingValues.BLOCK, R.string.website_settings_category_vr_ask,
                        R.string.website_settings_category_vr_blocked);
        }
        assert false; // NOTREACHED
        return null;
    }

    /**
     * Returns the resource id of the 24dp icon for a content type.
     */
    public static int getIcon(int contentType, SiteSettingsDelegate delegate) {
        return getResourceItem(contentType, delegate).getIcon();
    }

    /**
     * Returns a grey 24dp permission icon.
     *
     * @param context The Context for this drawable.
     * @param contentSettingsType The ContentSettingsType for this drawable. Returns null if the
     *         resource for this type cannot be found.
     * @param value The ContentSettingValues for this drawable. If ContentSettingValues.BLOCK, the
     *         returned icon will have a strike through it.
     * @return A grey 24dp {@link Drawable} for this content setting.
     */
    public static Drawable getContentSettingsIcon(Context context,
            @ContentSettingsType int contentSettingsType,
            @ContentSettingValues @Nullable Integer value, SiteSettingsDelegate delegate) {
        Drawable icon =
                SettingsUtils.getTintedIcon(context, getIcon(contentSettingsType, delegate));
        if (value != null && value == ContentSettingValues.BLOCK) {
            return getBlockedSquareIcon(context.getResources(), icon);
        }
        return icon;
    }

    /**
     * Returns a blue 24dp permission icon.
     *
     * @param context The Context for this drawable.
     * @param contentSettingsType The ContentSettingsType for this drawable. Returns null if the
     *         resource for this type cannot be found.
     * @param value The ContentSettingValues for this drawable. If ContentSettingValues.BLOCK, the
     *         returned icon will have a strike through it.
     * @param isIncognito Whether this icon should use the incognito color scheme.
     * @return A blue 24dp {@link Drawable} for this content setting.
     */
    public static Drawable getIconForOmnibox(Context context,
            @ContentSettingsType int contentSettingsType,
            @ContentSettingValues @Nullable Integer value, boolean isIncognito) {
        int color = isIncognito ? R.color.default_icon_color_blue_light
                                : R.color.default_icon_color_accent1_tint_list;
        Drawable icon =
                SettingsUtils.getTintedIcon(context, getIcon(contentSettingsType, null), color);
        if (value != null && value == ContentSettingValues.BLOCK) {
            return getBlockedSquareIcon(context.getResources(), icon);
        }
        return icon;
    }

    /**
     * @return A {@link Drawable} that is the blocked version of the square icon passed in.
     *         Achieved by adding a diagonal strike through the icon.
     */
    private static Drawable getBlockedSquareIcon(Resources resources, Drawable icon) {
        if (icon == null) return null;
        // Save color filter in order to re-apply later
        ColorFilter filter = icon.getColorFilter();

        // Create bitmap from drawable
        Bitmap iconBitmap = Bitmap.createBitmap(
                icon.getIntrinsicWidth(), icon.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(iconBitmap);
        int side = canvas.getWidth();
        assert side == canvas.getHeight();
        icon.setBounds(0, 0, side, side);
        icon.draw(canvas);

        // Thickness of the strikethrough line in pixels, relative to the icon size.
        float thickness = 0.08f * side;
        // Determines the axis bounds for where the line should start and finish.
        float padding = side * 0.15f;
        // The scaling ratio to get the axis bias. sin(45 degrees).
        float ratio = 0.7071f;
        // Calculated axis shift for the line in order to only be on one side of the transparent
        // line.
        float bias = (thickness / 2) * ratio;

        // Draw diagonal transparent line
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(Color.TRANSPARENT);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OUT));
        // Scale by 1.5 then shift up by half of bias in order to ensure no weird gap between lines
        // due to rounding.
        float halfBias = 0.5f * bias;
        paint.setStrokeWidth(1.5f * thickness);
        canvas.drawLine(padding + halfBias, padding - halfBias, side - padding + halfBias,
                side - padding - halfBias, paint);

        // Draw a strikethrough directly below.
        paint.setColor(Color.BLACK);
        paint.setXfermode(null);
        paint.setStrokeWidth(thickness);
        canvas.drawLine(padding - bias, padding + bias, side - padding - bias,
                side - padding + bias, paint);

        Drawable blocked = new BitmapDrawable(resources, iconBitmap);
        // Re-apply color filter
        blocked.setColorFilter(filter);
        return blocked;
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page
     * and in the global toggle at the top of a Website Settings page for a category.
     */
    public static int getTitleForCategory(
            @SiteSettingsCategory.Type int type, SiteSettingsDelegate delegate) {
        if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            return R.string.third_party_cookies_page_title;
        }
        return getTitle(SiteSettingsCategory.contentSettingsType(type), delegate);
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page
     * and in the global toggle at the top of a Website Settings page for a content type.
     */
    public static int getTitle(
            @ContentSettingsType int contentType, SiteSettingsDelegate delegate) {
        return getResourceItem(contentType, delegate).getTitle();
    }

    /**
     * Returns which ContentSetting the global default is set to, when enabled.
     * Either Ask/Allow. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static @ContentSettingValues @Nullable Integer getDefaultEnabledValue(int contentType) {
        return getResourceItem(contentType, null).getDefaultEnabledValue();
    }

    /**
     * Returns which ContentSetting the global default is set to, when disabled.
     * Usually Block. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static @ContentSettingValues @Nullable Integer getDefaultDisabledValue(int contentType) {
        return getResourceItem(contentType, null).getDefaultDisabledValue();
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a permission category.
     * @param value The ContentSetting for which we want the resource.
     */
    public static int getCategorySummary(@ContentSettingValues int value) {
        switch (value) {
            case ContentSettingValues.ALLOW:
                return R.string.website_settings_category_allowed;
            case ContentSettingValues.BLOCK:
                return R.string.website_settings_category_blocked;
            case ContentSettingValues.ASK:
                return R.string.website_settings_category_ask;
            default:
                return 0;
        }
    }

    /**
     * Returns the string resource id for a content type to show with a permission category.
     * @param enabled Whether the content type is enabled.
     */
    public static int getCategorySummary(int contentType, boolean enabled) {
        return getCategorySummary(enabled ? getDefaultEnabledValue(contentType)
                                          : getDefaultDisabledValue(contentType));
    }

    /**
     * Returns the string resource id for a given ContentSetting to show
     * with a particular website.
     * @param value The ContentSetting for which we want the resource.
     * @param contentSettingsType The ContentSettingsType for this string resource id.
     */
    public static int getSiteSummary(@ContentSettingValues @Nullable Integer value,
            @ContentSettingsType int contentSettingsType) {
        switch (value) {
            case ContentSettingValues.ALLOW:
                return contentSettingsType == ContentSettingsType.REQUEST_DESKTOP_SITE
                        ? R.string.website_settings_desktop_site_allow
                        : R.string.website_settings_permissions_allow;
            case ContentSettingValues.BLOCK:
                return contentSettingsType == ContentSettingsType.REQUEST_DESKTOP_SITE
                        ? R.string.website_settings_desktop_site_block
                        : R.string.website_settings_permissions_block;
            default:
                return 0; // We never show Ask as an option on individual permissions.
        }
    }

    /**
     * Returns the summary (resource id) to show when the content type is enabled.
     */
    public static int getEnabledSummary(int contentType, SiteSettingsDelegate delegate) {
        return getResourceItem(contentType, delegate).getEnabledSummary();
    }

    /**
     * Returns the summary (resource id) to show when the content type is disabled.
     */
    public static int getDisabledSummary(int contentType, SiteSettingsDelegate delegate) {
        return getResourceItem(contentType, delegate).getDisabledSummary();
    }

    /**
     * Returns the summary for Geolocation content settings when it is set to 'Allow' (by policy).
     */
    public static int getGeolocationAllowedSummary() {
        return R.string.website_settings_category_allowed;
    }

    /**
     * Returns the summary for Cookie content settings when it is allowed
     * except for those from third party sources.
     */
    public static int getCookieAllowedExceptThirdPartySummary() {
        return R.string.website_settings_category_allowed_except_third_party;
    }

    /**
     * Returns the blocked summary for the ads permission which should be used for display in the
     * site settings list only.
     */
    public static int getAdsBlockedListSummary() {
        return R.string.website_settings_category_ads_blocked_list;
    }

    /**
     * Returns the blocked summary for the clipboard permission which should be used for display in
     * the site settings list only.
     */
    public static int getClipboardBlockedListSummary() {
        return R.string.website_settings_category_clipboard_blocked_list;
    }

    /**
     * Returns the blocked summary for the sound permission which should be used for display in the
     * site settings list only.
     */
    public static int getSoundBlockedListSummary() {
        return R.string.website_settings_category_sound_blocked_list;
    }

    /**
     * Returns the allowed/blocked summary for the desktop site permission which should be used for
     * display in the site settings list only.
     */
    public static int getDesktopSiteListSummary(boolean enabled) {
        return enabled ? R.string.website_settings_category_desktop_site_allowed_list
                       : R.string.website_settings_category_desktop_site_blocked_list;
    }

    /**
     * Returns the allowed/blocked summary for the auto dark web content, which should be used for
     * display in the site settings list only.
     */
    public static int getAutoDarkWebContentListSummary(boolean enabled) {
        return enabled ? R.string.text_on : R.string.text_off;
    }

    /**
     * Returns the summary for the site data content setting which should be used for display in the
     * site settings list only.
     */
    public static int getSiteDataListSummary(boolean enabled) {
        return enabled ? R.string.site_settings_page_site_data_allowed_sub_label
                       : R.string.site_settings_page_site_data_blocked_sub_label;
    }

    /**
     * Returns the summary for the third-party cookie content setting which should be used for
     * display in the site settings list only.
     */
    public static int getThirdPartyCookieListSummary(@CookieControlsMode int cookieControlsMode) {
        switch (cookieControlsMode) {
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                return R.string.third_party_cookies_link_row_sub_label_disabled;
            case CookieControlsMode.INCOGNITO_ONLY:
                return R.string.third_party_cookies_link_row_sub_label_disabled_incognito;
            case CookieControlsMode.OFF:
                return R.string.third_party_cookies_link_row_sub_label_enabled;
        }
        assert false;
        return 0;
    }

    /**
     * Returns the resources IDs for descriptions for Allowed, Ask and Blocked states, in that
     * order, on a tri-state setting.
     *
     * @return An array of 3 resource IDs for descriptions for Allowed, Ask and
     *         Blocked states, in that order.
     */
    public static int[] getTriStateSettingDescriptionIDs(int contentType) {
        if (contentType == ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER) {
            int[] descriptionIDs = {
                    R.string.website_settings_category_protected_content_allowed_recommended,
                    R.string.website_settings_category_protected_content_ask,
                    R.string.website_settings_category_protected_content_blocked};
            return descriptionIDs;
        }

        assert false;
        return null;
    }
}
