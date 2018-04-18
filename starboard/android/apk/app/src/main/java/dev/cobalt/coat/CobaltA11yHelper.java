// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.widget.ExploreByTouchHelper;
import android.util.Log;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import java.util.BitSet;
import java.util.List;

/**
 * An ExploreByTouchHelper that create a virtual d-pad grid, so that Cobalt remains functional when
 * the TalkBack screen reader is enabled (which otherwise intercepts d-pad events for most
 * applications).
 */
class CobaltA11yHelper extends ExploreByTouchHelper {
  // These are from starboard/key.h
  private static final int SB_KEY_GAMEPAD_DPAD_UP = 0x800C;
  private static final int SB_KEY_GAMEPAD_DPAD_DOWN = 0x800D;
  private static final int SB_KEY_GAMEPAD_DPAD_LEFT = 0x800E;
  private static final int SB_KEY_GAMEPAD_DPAD_RIGHT = 0x800F;

  // The fake dimensions for the nine virtual views.
  // These values are arbitrary as long as the views stay on the screen.
  private static final int FAKE_VIEW_HEIGHT = 10;
  private static final int FAKE_VIEW_WIDTH = 10;

  private int previousFocusedViewId = 1;
  // This set tracks whether onPopulateNodeForVirtualView has been
  // called for each virtual view id.
  private final BitSet nodePopulatedSet = new BitSet(9);
  private final Handler handler = new Handler();
  private boolean hasInitialFocusBeenSet;

  public CobaltA11yHelper(View view) {
    super(view);
    ViewCompat.setAccessibilityDelegate(view, this);
  }

  private static native void nativeInjectKeyEvent(int key);

  @Override
  protected int getVirtualViewAt(float x, float y) {
    // This method is only required for touch or mouse interfaces.
    // Since we don't support either, we simply always return HOST_ID.
    return HOST_ID;
  }

  @Override
  protected void getVisibleVirtualViews(List<Integer> virtualViewIds) {
    if (!virtualViewIds.isEmpty()) {
      throw new RuntimeException("Expected empty list");
    }
    // We always have precisely 9 virtual views.
    for (int i = 1; i <= 9; i++) {
      virtualViewIds.add(i);
    }
  }

  /**
   * Returns the "patch number" for a given view id, given a focused view id.
   *
   * <p>A "patch number" is a 1-9 number that describes where the requestedViewId is now located on
   * an X-Y grid, given the focusedViewId.
   *
   * <p>Patch number grid:
   * (0,0)----->X
   *   |+-+-+-+
   *   ||1|2|3|
   *   |+-+-+-|
   *   ||4|5|6|
   *   |+-+-+-|
   *   ||7|8|9|
   *   |+-+-+-+
   *  \./ Y
   *
   * <p>As focus changes, the locations of the views are moved so the focused view is always in the
   * middle (patch number 5) and all of the other views always in the same relative position with
   * respect to each other (with those on the edges adjacent to those on the opposite edges --
   * wrapping around).
   *
   * <p>5 is returned whenever focusedViewId = requestedViewId
   */
  private static int getPatchNumber(int focusedViewId, int requestedViewId) {
    // The (x,y) the focused view has in the 9 patch where 5 is in the middle.
    int focusedX = (focusedViewId - 1) % 3;
    int focusedY = (focusedViewId - 1) / 3;

    // x and y offsets of focused view where middle is (0, 0)
    int focusedRelativeToCenterX = focusedX - 1;
    int focusedRelativeToCenterY = focusedY - 1;

    // The (x,y) the requested view has in the 9 patch where 5 is in the middle.
    int requestedX = (requestedViewId - 1) % 3;
    int requestedY = (requestedViewId - 1) / 3;

    // x and y offsets of requested view where middle is (0, 0)
    int requestedRelativeToCenterX = requestedX - 1;
    int requestedRelativeToCenterY = requestedY - 1;

    // The (x,y) that the requested view has in the 9 patch when focusedViewId
    // is in the middle.
    int translatedRequestedX = (1 + 3 + requestedRelativeToCenterX - focusedRelativeToCenterX) % 3;
    int translatedRequestedY = (1 + 3 + requestedRelativeToCenterY - focusedRelativeToCenterY) % 3;

    return (translatedRequestedY * 3) + translatedRequestedX + 1;
  }

  private void maybeInjectEvent(int currentFocusedViewId) {
    switch (getPatchNumber(previousFocusedViewId, currentFocusedViewId)) {
      case 5:
        // no move;
        break;
      case 2:
        nativeInjectKeyEvent(SB_KEY_GAMEPAD_DPAD_UP);
        break;
      case 4:
        nativeInjectKeyEvent(SB_KEY_GAMEPAD_DPAD_LEFT);
        break;
      case 6:
        nativeInjectKeyEvent(SB_KEY_GAMEPAD_DPAD_RIGHT);
        break;
      case 8:
        nativeInjectKeyEvent(SB_KEY_GAMEPAD_DPAD_DOWN);
        break;
      default:
        // TODO: Could support diagonal movements, although it's likely
        // not possible to reach this.
        break;
    }
    previousFocusedViewId = currentFocusedViewId;
  }

  @Override
  protected void onPopulateNodeForVirtualView(int virtualViewId, AccessibilityNodeInfoCompat node) {
    int focusedViewId = getAccessibilityFocusedVirtualViewId();

    if (focusedViewId < 1 || focusedViewId > 9) {
      // If this is not one of our nine-patch views, it's probably HOST_ID
      // In any case, assume there is no focus change.
      focusedViewId = previousFocusedViewId;
    }

    // onPopulateNodeForVirtualView() gets called at least once every
    // time the focused view changes. So see if it's changed since the
    // last time we've been called and inject an event if so.
    maybeInjectEvent(focusedViewId);

    int patchNumber = getPatchNumber(focusedViewId, virtualViewId);

    int x = (patchNumber - 1) % 3;
    int y = (patchNumber - 1) / 3;

    // Note that the specific bounds here are arbitrary. The importance
    // is the relative bounds to each other.
    node.setBoundsInParent(new Rect(
        x * FAKE_VIEW_WIDTH,
        y * FAKE_VIEW_HEIGHT,
        x * FAKE_VIEW_WIDTH + FAKE_VIEW_WIDTH,
        y * FAKE_VIEW_HEIGHT + FAKE_VIEW_HEIGHT));
    node.setText("");

    if (virtualViewId >= 1 || virtualViewId <= 9) {
      nodePopulatedSet.set(virtualViewId - 1);
    }
    if (!hasInitialFocusBeenSet && nodePopulatedSet.cardinality() == 9) {
      // Once the ExploreByTouchHelper knows about all of our virtual views,
      // but not before, ask that the accessibility focus be moved from
      // it's initial position on HOST_ID to the one we want to start with.
      hasInitialFocusBeenSet = true;
      handler.post(
          new Runnable() {
            @Override
            public void run() {
              sendEventForVirtualView(
                  previousFocusedViewId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
            }
          });
    }
  }

  @Override
  protected boolean onPerformActionForVirtualView(int virtualViewId, int action, Bundle arguments) {
    return false;
  }

  /** A simple equivilent to Assert.assertEquals so we don't depend on junit */
  private static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new RuntimeException("Expected " + expected + " actual " + actual);
    }
  }

  /**
   * Unit test for getPatchNumber().
   *
   * <p>As of this writing, the Java portion of the Cobalt build has no unit test mechanism.
   *
   * <p>To run this test, simply call it from application start and start the application.
   *
   * <p>TODO: Move this to a real unit test location when one exists.
   */
  private static void testGetPatchNumber() {
    Log.i(TAG, "+testGetPatchNumber");

    assertEquals(1, getPatchNumber(5, 1));
    assertEquals(2, getPatchNumber(5, 2));
    assertEquals(3, getPatchNumber(5, 3));
    assertEquals(4, getPatchNumber(5, 4));
    assertEquals(5, getPatchNumber(5, 5));
    assertEquals(6, getPatchNumber(5, 6));
    assertEquals(7, getPatchNumber(5, 7));
    assertEquals(8, getPatchNumber(5, 8));
    assertEquals(9, getPatchNumber(5, 9));

    for (int i = 1; i <= 9; i++) {
      assertEquals(5, getPatchNumber(i, i));
    }

    assertEquals(5, getPatchNumber(1, 1));
    assertEquals(6, getPatchNumber(1, 2));
    assertEquals(4, getPatchNumber(1, 3));
    assertEquals(8, getPatchNumber(1, 4));
    assertEquals(9, getPatchNumber(1, 5));
    assertEquals(7, getPatchNumber(1, 6));
    assertEquals(2, getPatchNumber(1, 7));
    assertEquals(3, getPatchNumber(1, 8));
    assertEquals(1, getPatchNumber(1, 9));

    assertEquals(9, getPatchNumber(9, 1));
    assertEquals(7, getPatchNumber(9, 2));
    assertEquals(8, getPatchNumber(9, 3));
    assertEquals(3, getPatchNumber(9, 4));
    assertEquals(1, getPatchNumber(9, 5));
    assertEquals(2, getPatchNumber(9, 6));
    assertEquals(6, getPatchNumber(9, 7));
    assertEquals(4, getPatchNumber(9, 8));
    assertEquals(5, getPatchNumber(9, 9));
    Log.i(TAG, "-testGetPatchNumber");
  }
}
