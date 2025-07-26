// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.content_public.browser.WebContents;

/** Java-side copy of translate::ContentTranslateDriver::TranslationObserver. */
public interface TranslationObserver {

    @CalledByNative
    default void onIsPageTranslatedChanged(WebContents contents) {}

    @CalledByNative
    default void onPageTranslated(
            @JniType("std::string") String sourceLanguage,
            @JniType("std::string") String translatedLanguage,
            int errorCode) {}
}
