// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwKeyboardShortcuts;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;

/**
 * {@link org.chromium.android_webview.AwKeyboardShortcuts} tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Features.EnableFeatures({AwFeatures.WEBVIEW_ZOOM_KEYBOARD_SHORTCUTS})
@Batch(Batch.PER_CLASS)
public class AwKeyboardShortcutsTest {
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwContents mAwContents;

    @Before
    public void setUp() {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        mAwContents =
                Mockito.spy(mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                                    .getAwContents());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testCtrlPlusZoomIn() {
        executeCtrlPlus();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testCtrlShiftPlusZoomIn() {
        executeCtrlShiftPlus();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testCtrlEqualsZoomIn() {
        executeCtrlEquals();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testCtrlShiftEqualsZoomIn() {
        executeCtrlShiftEquals();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testKeyEventZoomInZoomIn() {
        executeZoomInKey();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testCtrlMinusZoomOut() {
        executeCtrlMinus();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomOut();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testKeyEventZoomOutZoomOut() {
        executeZoomOutKey();
        Mockito.verify(mAwContents, Mockito.times(1)).zoomOut();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testPlusWithNoCtrlNoZoomIn() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PLUS, 0, 0);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
        Mockito.verify(mAwContents, Mockito.never()).zoomIn();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testMinusWithNoCtrlNoZoomOut() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MINUS, 0, 0);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
        Mockito.verify(mAwContents, Mockito.never()).zoomOut();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "This test is disabled on Android N because of https://crbug.com/1414082")
    public void
    testWebViewZoomNotSupported() {
        mAwContents.getSettings().setSupportZoom(false);
        executeAllZoomShortcuts();
        Mockito.verify(mAwContents, Mockito.never()).zoomIn();
        Mockito.verify(mAwContents, Mockito.never()).zoomOut();
        mAwContents.getSettings().setSupportZoom(true);
    }

    private void executeCtrlPlus() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PLUS, 0,
                KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeCtrlShiftPlus() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PLUS, 0,
                KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON | KeyEvent.META_SHIFT_ON
                        | KeyEvent.META_SHIFT_LEFT_ON);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeCtrlEquals() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_EQUALS, 0,
                KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeCtrlShiftEquals() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_EQUALS, 0,
                KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON | KeyEvent.META_SHIFT_ON);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeZoomInKey() {
        KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ZOOM_IN);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeCtrlMinus() {
        KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MINUS, 0,
                KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeZoomOutKey() {
        KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ZOOM_OUT);
        AwKeyboardShortcuts.onKeyDown(keyEvent, mAwContents);
    }

    private void executeAllZoomShortcuts() {
        executeCtrlPlus();
        executeCtrlShiftPlus();
        executeCtrlEquals();
        executeCtrlShiftEquals();
        executeZoomInKey();
        executeCtrlMinus();
        executeZoomOutKey();
    }
}
