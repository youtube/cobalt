// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 *
 * IMPORTANT: Clients that use {@link ResourceBundle} MUST call either
 * {@link ResourceBundle#setAvailablePakLocales(String[], String[])} or
 * {@link ResourceBundle#setNoAvailableLocalePaks()} before calling the getters in this class.
 */
@JNINamespace("ui")
public final class ResourceBundle {
    private static final String TAG = "ResourceBundle";
    private static String[] sAvailableLocales;

    private ResourceBundle() {}

    /**
     * Called when there are no locale pak files available.
     */
    @CalledByNative
    public static void setNoAvailableLocalePaks() {
        assert sAvailableLocales == null;
        sAvailableLocales = new String[] {};
    }

    /**
     * Sets the available locale pak files.
     * @param uncompressed Locales that have pak files.
     */
    public static void setAvailablePakLocales(String[] locales) {
        assert sAvailableLocales == null;
        sAvailableLocales = locales;
    }

    public static void clearAvailablePakLocalesForTesting() {
        sAvailableLocales = null;
    }

    /**
     * Return the list of available locales.
     * @return The correct locale list for this build.
     */
    public static String[] getAvailableLocales() {
        assert sAvailableLocales != null;
        return sAvailableLocales;
    }

    /**
     * Return the location of a locale-specific .pak file asset.
     *
     * @param locale Chromium locale name.
     * @param inBundle If true, return the path of the uncompressed .pak file
     *                 containing Chromium UI strings within app bundles. If
     *                 false, return the path of the WebView UI strings instead.
     * @param logError Logs if the file is not found.
     * @return Asset path to .pak file, or null if the locale is not supported.
     */
    @CalledByNative
    private static String getLocalePakResourcePath(
            String locale, boolean inBundle, boolean logError) {
        if (sAvailableLocales == null) {
            // Locales may be null in unit tests.
            return null;
        }
        if (Arrays.binarySearch(sAvailableLocales, locale) < 0) {
            // This locale is not supported by Chromium.
            return null;
        }
        String pathPrefix = "assets/stored-locales/";
        if (inBundle) {
            if (locale.equals("en-US")) {
                pathPrefix = "assets/fallback-locales/";
            } else {
                String lang = LocalizationUtils.getSplitLanguageForAndroid(
                        LocaleUtils.toBaseLanguage(locale));
                pathPrefix = "assets/locales#lang_" + lang + "/";
            }
        }
        String assetPath = pathPrefix + locale + ".pak";
        AssetManager manager = ContextUtils.getApplicationContext().getAssets();
        // The file may not exist if the language split for this locale has not been installed
        // yet, so make sure it exists before returning the asset path.
        try (AssetFileDescriptor afd = manager.openNonAssetFd(assetPath)) {
            return assetPath;
        } catch (IOException e) {
            // Fallback for apk targets.
            // TODO(crbug.com/1176290): Remove the need for this fallback logic.
            String fallbackPath = "assets/locales/" + locale + ".pak";
            try (AssetFileDescriptor afd = manager.openNonAssetFd(fallbackPath)) {
                return fallbackPath;
            } catch (IOException e2) {
            }
            if (logError) {
                Log.e(TAG, "path=%s", assetPath, e);
            }
            return null;
        }
    }
}
