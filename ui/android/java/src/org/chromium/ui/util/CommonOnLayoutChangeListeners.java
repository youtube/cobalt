// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.view.View;
import android.view.View.OnLayoutChangeListener;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/** A utility class containing common {@link View.OnLayoutChangeListener} factory methods. */
@NullMarked
public class CommonOnLayoutChangeListeners {
    private CommonOnLayoutChangeListeners() {}

    /** A functional interface to be invoked when layout parameters of a view change. */
    @FunctionalInterface
    public interface OnLayoutChanged {
        /** Called when the layout parameters shift. */
        void onLayoutChanged(View v, @Px int left, @Px int top, @Px int right, @Px int bottom);
    }

    @FunctionalInterface
    private interface LayoutFilter {
        boolean shouldTrigger(
                @Px int left,
                @Px int top,
                @Px int right,
                @Px int bottom,
                @Px int oldLeft,
                @Px int oldTop,
                @Px int oldRight,
                @Px int oldBottom);
    }

    private static final LayoutFilter BOUNDS_CHANGED =
            (l, t, r, b, ol, ot, or, ob) -> didBoundsChange(l, t, r, b, ol, ot, or, ob);

    private static final LayoutFilter SIZE_CHANGED =
            (l, t, r, b, ol, ot, or, ob) -> didSizeChange(l, t, r, b, ol, ot, or, ob);

    private static final LayoutFilter HEIGHT_CHANGED =
            (l, t, r, b, ol, ot, or, ob) -> didHeightChange(t, b, ot, ob);

    private static final LayoutFilter WIDTH_CHANGED =
            (l, t, r, b, ol, ot, or, ob) -> didWidthChange(l, r, ol, or);

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link Runnable} when
     * the view's layout bounds change. Duplicate layout events with unchanged bounds are filtered.
     *
     * @param runnable The runnable to run when layout bounds change.
     */
    public static OnLayoutChangeListener createBoundsChangedListener(Runnable runnable) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (BOUNDS_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) runnable.run();
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link OnLayoutChanged}
     * callback when the view's layout bounds change. Duplicate layout events with unchanged bounds
     * are filtered.
     *
     * @param callback The callback to execute when layout bounds change.
     */
    public static OnLayoutChangeListener createBoundsChangedListener(OnLayoutChanged callback) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (BOUNDS_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) {
                callback.onLayoutChanged(v, l, t, r, b);
            }
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link Runnable} when
     * the view's layout size changes. Duplicate layout events with unchanged sizes are filtered.
     *
     * @param runnable The runnable to run when layout size changes.
     */
    public static OnLayoutChangeListener createSizeChangedListener(Runnable runnable) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (SIZE_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) runnable.run();
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link OnLayoutChanged}
     * callback when the view's layout size changes. Duplicate layout events with unchanged sizes
     * are filtered.
     *
     * @param callback The callback to execute when layout size changes.
     */
    public static OnLayoutChangeListener createSizeChangedListener(OnLayoutChanged callback) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (SIZE_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) {
                callback.onLayoutChanged(v, l, t, r, b);
            }
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link Runnable} when
     * the view's layout height changes. Duplicate layout events with unchanged heights are
     * filtered.
     *
     * @param runnable The runnable to run when layout height changes.
     */
    public static OnLayoutChangeListener createHeightChangedListener(Runnable runnable) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (HEIGHT_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) runnable.run();
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link OnLayoutChanged}
     * callback when the view's layout height changes. Duplicate layout events with unchanged
     * heights are filtered.
     *
     * @param callback The callback to execute when layout height changes.
     */
    public static OnLayoutChangeListener createHeightChangedListener(OnLayoutChanged callback) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (HEIGHT_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) {
                callback.onLayoutChanged(v, l, t, r, b);
            }
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link Runnable} when
     * the view's layout width changes. Duplicate layout events with unchanged widths are filtered.
     *
     * @param runnable The runnable to run when layout width changes.
     */
    public static OnLayoutChangeListener createWidthChangedListener(Runnable runnable) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (WIDTH_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) runnable.run();
        };
    }

    /**
     * Creates a {@link View.OnLayoutChangeListener} that executes the given {@link OnLayoutChanged}
     * callback when the view's layout width changes. Duplicate layout events with unchanged widths
     * are filtered.
     *
     * @param callback The callback to execute when layout width changes.
     */
    public static OnLayoutChangeListener createWidthChangedListener(OnLayoutChanged callback) {
        return (v, l, t, r, b, ol, ot, or, ob) -> {
            if (WIDTH_CHANGED.shouldTrigger(l, t, r, b, ol, ot, or, ob)) {
                callback.onLayoutChanged(v, l, t, r, b);
            }
        };
    }

    private static boolean didBoundsChange(
            @Px int left,
            @Px int top,
            @Px int right,
            @Px int bottom,
            @Px int oldLeft,
            @Px int oldTop,
            @Px int oldRight,
            @Px int oldBottom) {
        return left != oldLeft || top != oldTop || right != oldRight || bottom != oldBottom;
    }

    private static boolean didSizeChange(
            @Px int left,
            @Px int top,
            @Px int right,
            @Px int bottom,
            @Px int oldLeft,
            @Px int oldTop,
            @Px int oldRight,
            @Px int oldBottom) {
        return (right - left) != (oldRight - oldLeft) || (bottom - top) != (oldBottom - oldTop);
    }

    private static boolean didHeightChange(
            @Px int top, @Px int bottom, @Px int oldTop, @Px int oldBottom) {
        return (bottom - top) != (oldBottom - oldTop);
    }

    private static boolean didWidthChange(
            @Px int left, @Px int right, @Px int oldLeft, @Px int oldRight) {
        return (right - left) != (oldRight - oldLeft);
    }
}
