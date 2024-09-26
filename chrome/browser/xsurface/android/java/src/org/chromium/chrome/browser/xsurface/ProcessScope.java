// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScopeDependencyProvider;

/**
 * Implemented internally.
 *
 * Used to initialize singleton-level dependencies for xsurface. Also provides surface-level
 * dependencies that depend on the singleton dependencies.
 **/
public interface ProcessScope {
    /**
     * To be called after a login state change event, will cause the next SurfaceScope to use fresh
     * account-level dependencies.
     */
    default void resetAccount() {}

    /**
     * Returns a SurfaceScope which should be one per Surface. That Surface can have multiple
     * HybridListRenderers and SurfaceRenderers within its UI.
     *
     * @param dependencyProvider Provider for activity-scoped dependencies.
     **/
    @Deprecated
    @Nullable
    default SurfaceScope obtainSurfaceScope(SurfaceScopeDependencyProvider dependencyProvider) {
        return null;
    }

    // TODO(b/269234249): Don't call from Chrome yet, it's not implemented.
    /**
     * Returns a SurfaceScope which should be one per Surface. That Surface can have multiple
     * HybridListRenderers and SurfaceRenderers within its UI.
     *
     * @param dependencyProvider Provider for activity-scoped dependencies.
     **/
    @Nullable
    default FeedSurfaceScope obtainFeedSurfaceScope(
            FeedSurfaceScopeDependencyProvider dependencyProvider) {
        return null;
    }

    @Nullable
    default ImageCacheHelper provideImageCacheHelper() {
        return null;
    }

    @Nullable
    default ReliabilityLoggingTestUtil provideReliabilityLoggingTestUtil() {
        return null;
    }
}
