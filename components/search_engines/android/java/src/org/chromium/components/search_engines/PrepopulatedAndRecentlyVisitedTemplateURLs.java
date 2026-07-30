// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** A container for prepopulated and recently visited search engines. */
@NullMarked
public class PrepopulatedAndRecentlyVisitedTemplateURLs {
    private final List<TemplateUrl> mPrepopulatedUrls;
    private final List<TemplateUrl> mRecentlyVisitedUrls;

    @VisibleForTesting
    public PrepopulatedAndRecentlyVisitedTemplateURLs(
            List<TemplateUrl> prepopulatedUrls, List<TemplateUrl> recentlyVisitedUrls) {
        mPrepopulatedUrls = prepopulatedUrls;
        mRecentlyVisitedUrls = recentlyVisitedUrls;
    }

    @CalledByNative
    private static PrepopulatedAndRecentlyVisitedTemplateURLs create(
            @JniType("std::vector<const TemplateURL*>") List<TemplateUrl> prepopulatedUrls,
            @JniType("std::vector<const TemplateURL*>") List<TemplateUrl> recentlyVisitedUrls) {
        return new PrepopulatedAndRecentlyVisitedTemplateURLs(
                prepopulatedUrls, recentlyVisitedUrls);
    }

    public List<TemplateUrl> getPrepopulatedUrls() {
        return mPrepopulatedUrls;
    }

    public List<TemplateUrl> getRecentlyVisitedUrls() {
        return mRecentlyVisitedUrls;
    }
}
