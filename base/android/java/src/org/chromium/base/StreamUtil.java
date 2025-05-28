// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.Closeable;
import java.io.IOException;

/** Helper methods to deal with stream related tasks. */
@NullMarked
public class StreamUtil {
    /**
     * Handle closing a {@link java.io.Closeable} via {@link java.io.Closeable#close()} and catch
     * the potentially thrown {@link java.io.IOException}.
     *
     * @param closeable The Closeable to be closed.
     */
    public static void closeQuietly(@Nullable Closeable closeable) {
        if (closeable == null) return;

        try {
            closeable.close();
        } catch (IOException ex) {
            // Ignore the exception on close.
        }
    }
}
