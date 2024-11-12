// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import java.util.List;

/** Utility methods for {@link CachedFlag}s. */
public class CachedFlagUtils {
    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheNativeFlags(List<CachedFlag>... listsOfFeaturesToCache) {
        if (listsOfFeaturesToCache.length == 0) return;

        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (final List<CachedFlag> featuresToCache : listsOfFeaturesToCache) {
            for (CachedFlag feature : featuresToCache) {
                feature.writeCacheValueToEditor(editor);
            }
        }
        editor.apply();
    }

    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheFieldTrialParameters(
            List<CachedFieldTrialParameter<?>>... listsOfParameters) {
        if (listsOfParameters.length == 0) return;

        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (final List<CachedFieldTrialParameter<?>> parameters : listsOfParameters) {
            for (final CachedFieldTrialParameter<?> parameter : parameters) {
                parameter.writeCacheValueToEditor(editor);
            }
        }
        editor.apply();
    }
}
