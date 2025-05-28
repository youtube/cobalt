// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link NewTabAnimationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewTabAnimationUtilsUnitTest {
    @Test
    public void testUpdateRects_Rtl_topAligned() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(
                initialRect, finalRect, /* isRtl= */ true, /* isTopAligned= */ true);

        Rect expectedInitialRect = new Rect(80, -100, 150, -40);
        Rect expectedFinalRect = new Rect(-235, -100, 150, 230);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }

    @Test
    public void testUpdateRects_Rtl_bottomAligned() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(
                initialRect, finalRect, /* isRtl= */ true, /* isTopAligned= */ false);

        Rect expectedInitialRect = new Rect(80, 140, 150, 200);
        Rect expectedFinalRect = new Rect(-235, -130, 150, 200);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }

    @Test
    public void testUpdateRects_Ltr_topAligned() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(
                initialRect, finalRect, /* isRtl= */ false, /* isTopAligned= */ true);

        Rect expectedInitialRect = new Rect(-200, -100, -130, -40);
        Rect expectedFinalRect = new Rect(-200, -100, 185, 230);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }

    @Test
    public void testUpdateRects_Ltr_bottomAligned() {
        Rect initialRect = new Rect();
        Rect finalRect = new Rect(-200, -100, 150, 200);

        NewTabAnimationUtils.updateRects(
                initialRect, finalRect, /* isRtl= */ false, /* isTopAligned= */ false);

        Rect expectedInitialRect = new Rect(-200, 140, -130, 200);
        Rect expectedFinalRect = new Rect(-200, -130, 185, 200);

        assertEquals(expectedInitialRect, initialRect);
        assertEquals(expectedFinalRect, finalRect);
    }
}
