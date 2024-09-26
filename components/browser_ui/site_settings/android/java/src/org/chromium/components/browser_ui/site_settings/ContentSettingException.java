// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/**
 * Exception information for a given origin.
 */
public class ContentSettingException implements Serializable {
    private final @ContentSettingsType int mContentSettingType;
    private final String mPrimaryPattern;
    private final String mSecondaryPattern;
    private final @ContentSettingValues @Nullable Integer mContentSetting;
    // TODO(crbug.com/1344877): Convert {@link #mSource} to enum to enable merging {@link #mSource}
    // and {@link #mIsEmbargoed}.
    private final String mSource;
    private final boolean mIsEmbargoed;

    /**
     * Construct a ContentSettingException.
     * @param type The content setting type this exception covers.
     * @param primaryPattern The primary host/domain pattern this exception covers.
     * @param secondaryPattern The secondary host/domain pattern this exception covers.
     * @param setting The setting for this exception, e.g. ALLOW or BLOCK.
     * @param source The source for this exception.
     * @param isEmbargoed Whether the site is under embargo for {@link type}.
     */
    public ContentSettingException(@ContentSettingsType int type, String primaryPattern,
            String secondaryPattern, @ContentSettingValues @Nullable Integer setting, String source,
            boolean isEmbargoed) {
        mContentSettingType = type;
        mPrimaryPattern = primaryPattern;
        mSecondaryPattern = secondaryPattern;
        mContentSetting = setting;
        mSource = source;
        mIsEmbargoed = isEmbargoed;
    }

    /**
     * Construct a ContentSettingException.
     * Same as above but defaults secondaryPattern to wildcard.
     */
    public ContentSettingException(@ContentSettingsType int type, String primaryPattern,
            @ContentSettingValues @Nullable Integer setting, String source, boolean isEmbargoed) {
        this(type, primaryPattern, SITE_WILDCARD, setting, source, isEmbargoed);
    }

    public String getPrimaryPattern() {
        return mPrimaryPattern;
    }

    public String getSecondaryPattern() {
        return mSecondaryPattern;
    }

    private @NonNull String getSecondaryPatternSafe() {
        return (mSecondaryPattern == null) ? SITE_WILDCARD : mSecondaryPattern;
    }

    public String getSource() {
        return mSource;
    }

    public @ContentSettingValues @Nullable Integer getContentSetting() {
        return mContentSetting;
    }

    public @ContentSettingsType int getContentSettingType() {
        return mContentSettingType;
    }

    public boolean isEmbargoed() {
        return mIsEmbargoed;
    }

    /**
     * Sets the content setting value for this exception.
     */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSettingValues int value) {
        WebsitePreferenceBridge.setContentSettingCustomScope(browserContextHandle,
                mContentSettingType, mPrimaryPattern, getSecondaryPatternSafe(), value);
    }
}
