// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;

import android.view.View;
import android.view.View.OnLayoutChangeListener;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link CommonOnLayoutChangeListeners}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CommonOnLayoutChangeListenersTest {
    private View mView;

    @Before
    public void setUp() {
        mView = new View(ApplicationProvider.getApplicationContext());
    }

    @Test
    public void testLayoutBoundsChanged() {
        AtomicInteger callCount = new AtomicInteger(0);
        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createBoundsChangedListener(
                        callCount::incrementAndGet);

        // Run when bounds changed:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 50, 50);
        assertEquals(1, callCount.get());

        // Run again when bounds are different:
        listener.onLayoutChange(mView, 0, 0, 200, 100, 0, 0, 100, 100);
        assertEquals(2, callCount.get());
    }

    @Test
    public void testLayoutBoundsUnchanged() {
        AtomicInteger callCount = new AtomicInteger(0);
        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createBoundsChangedListener(
                        callCount::incrementAndGet);

        // Does not run when bounds are exactly the same:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 100, 100);
        assertEquals(0, callCount.get());

        // Run when bounds changed:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 50, 50);
        assertEquals(1, callCount.get());

        // Does not run when bounds match the new coordinates:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 100, 100);
        assertEquals(1, callCount.get());
    }

    @Test
    public void testOnLayoutChangedCallback() {
        AtomicInteger callCount = new AtomicInteger(0);
        int[] capturedBounds = new int[4];
        final View[] capturedView = new View[1];

        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createBoundsChangedListener(
                        (v, left, top, right, bottom) -> {
                            callCount.incrementAndGet();
                            capturedView[0] = v;
                            capturedBounds[0] = left;
                            capturedBounds[1] = top;
                            capturedBounds[2] = right;
                            capturedBounds[3] = bottom;
                        });

        // Does not run when bounds are exactly the same:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 100, 100);
        assertEquals(0, callCount.get());

        // Run when bounds changed:
        listener.onLayoutChange(mView, 10, 20, 110, 120, 0, 0, 50, 50);
        assertEquals(1, callCount.get());
        assertEquals(mView, capturedView[0]);
        assertEquals(10, capturedBounds[0]);
        assertEquals(20, capturedBounds[1]);
        assertEquals(110, capturedBounds[2]);
        assertEquals(120, capturedBounds[3]);
    }

    @Test
    public void testLayoutSizeChanged() {
        AtomicInteger callCount = new AtomicInteger(0);
        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createSizeChangedListener(callCount::incrementAndGet);

        // Run when size changed:
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 50, 50);
        assertEquals(1, callCount.get());

        // Run when size changed but same position/bounds shift:
        listener.onLayoutChange(mView, 10, 10, 110, 110, 0, 0, 100, 100);
        assertEquals(1, callCount.get());

        // Run when size changes:
        listener.onLayoutChange(mView, 10, 10, 120, 110, 10, 10, 110, 110);
        assertEquals(2, callCount.get());
    }

    @Test
    public void testOnSizeChangedCallback() {
        AtomicInteger callCount = new AtomicInteger(0);
        int[] capturedBounds = new int[4];
        final View[] capturedView = new View[1];

        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createSizeChangedListener(
                        (v, left, top, right, bottom) -> {
                            callCount.incrementAndGet();
                            capturedView[0] = v;
                            capturedBounds[0] = left;
                            capturedBounds[1] = top;
                            capturedBounds[2] = right;
                            capturedBounds[3] = bottom;
                        });

        // Run when size changed:
        listener.onLayoutChange(mView, 10, 20, 110, 130, 0, 0, 50, 50);
        assertEquals(1, callCount.get());
        assertEquals(mView, capturedView[0]);
        assertEquals(10, capturedBounds[0]);
        assertEquals(20, capturedBounds[1]);
        assertEquals(110, capturedBounds[2]);
        assertEquals(130, capturedBounds[3]);

        // Does not run when size is unchanged, even if bounds changed (shift without resizing):
        listener.onLayoutChange(mView, 20, 30, 120, 140, 10, 20, 110, 130);
        assertEquals(1, callCount.get());
    }

    @Test
    public void testLayoutHeightChanged() {
        AtomicInteger callCount = new AtomicInteger(0);
        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createHeightChangedListener(
                        callCount::incrementAndGet);

        // Run when height changed (width same):
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 100, 50);
        assertEquals(1, callCount.get());

        // Does not run when width changed but height same:
        listener.onLayoutChange(mView, 0, 0, 200, 100, 0, 0, 100, 100);
        assertEquals(1, callCount.get());

        // Run when height changes:
        listener.onLayoutChange(mView, 0, 0, 200, 200, 0, 0, 200, 100);
        assertEquals(2, callCount.get());
    }

    @Test
    public void testLayoutWidthChanged() {
        AtomicInteger callCount = new AtomicInteger(0);
        OnLayoutChangeListener listener =
                CommonOnLayoutChangeListeners.createWidthChangedListener(
                        callCount::incrementAndGet);

        // Run when width changed (height same):
        listener.onLayoutChange(mView, 0, 0, 100, 100, 0, 0, 50, 100);
        assertEquals(1, callCount.get());

        // Does not run when height changed but width same:
        listener.onLayoutChange(mView, 0, 0, 100, 200, 0, 0, 100, 100);
        assertEquals(1, callCount.get());

        // Run when width changes:
        listener.onLayoutChange(mView, 0, 0, 200, 200, 0, 0, 100, 200);
        assertEquals(2, callCount.get());
    }
}
