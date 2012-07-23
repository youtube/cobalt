// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.util.Locale;

/**
 * This class provides the locale related methods for the native library.
 */
class LocaleUtils {

    private LocaleUtils() { /* cannot be instantiated */ }

    /**
     * @return the default locale.
     */
    @CalledByNative
    public static String getDefaultLocale() {
        Locale locale = Locale.getDefault();
        String language = locale.getLanguage();
        String country = locale.getCountry();
        return country.isEmpty() ? language : language + "-" + country;
    }
}
