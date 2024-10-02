// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for the DelayedScreenLockIntentHandlerTest class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DelayedScreenLockIntentHandlerTest {
    @Mock
    private Context mApplicationContextMock;
    @Mock
    private Activity mActivityMock;

    private DelayedScreenLockIntentHandler mIntentHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIntentHandler = new DelayedScreenLockIntentHandler(mActivityMock);
        ContextUtils.initApplicationContextForTests(mApplicationContextMock);
    }

    @Test
    public void testReceiveBroadcast() {
        final Intent deferredIntent = new Intent();
        final Intent intent = new Intent(Intent.ACTION_USER_PRESENT);

        mIntentHandler.updateDeferredIntent(deferredIntent);
        mIntentHandler.onReceive(null, intent);

        verifyRegisterReceiverCall();
        verify(mActivityMock).startActivity(deferredIntent);
        verify(mApplicationContextMock).unregisterReceiver(mIntentHandler);
    }

    @Test
    public void testReceiveBroadcastTwice() {
        final Intent deferredIntent = new Intent();
        final Intent intent = new Intent(Intent.ACTION_USER_PRESENT);

        mIntentHandler.updateDeferredIntent(deferredIntent);
        mIntentHandler.onReceive(null, intent);
        mIntentHandler.onReceive(null, intent);

        verifyRegisterReceiverCall();
        verify(mActivityMock).startActivity(deferredIntent);
        verify(mApplicationContextMock).unregisterReceiver(mIntentHandler);
    }

    @Test
    public void testSecondDeferredIntentAction() {
        final Intent deferredIntent1 = new Intent();
        final Intent deferredIntent2 = new Intent();
        final Intent intent = new Intent(Intent.ACTION_USER_PRESENT);

        mIntentHandler.updateDeferredIntent(deferredIntent1);
        mIntentHandler.updateDeferredIntent(deferredIntent2);
        mIntentHandler.onReceive(null, intent);

        verifyRegisterReceiverCall();
        verify(mActivityMock).startActivity(deferredIntent2);
        verify(mApplicationContextMock).unregisterReceiver(mIntentHandler);
    }

    @Test
    public void testNonExpectedIntentAction() {
        mIntentHandler.updateDeferredIntent(new Intent());
        try {
            mIntentHandler.onReceive(null, new Intent());
        } catch (AssertionError assertError) {
            // Ignore AssertErrors
        }

        verifyRegisterReceiverCall();
        verify(mActivityMock, never()).startActivity(any(Intent.class));
        verify(mApplicationContextMock, never()).unregisterReceiver(mIntentHandler);
    }

    public void verifyRegisterReceiverCall() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            verify(mApplicationContextMock)
                    .registerReceiver(
                            eq(mIntentHandler), any(IntentFilter.class), isNull(), isNull(), eq(0));
        } else {
            verify(mApplicationContextMock)
                    .registerReceiver(
                            eq(mIntentHandler), any(IntentFilter.class), isNull(), isNull());
        }
    }
}
