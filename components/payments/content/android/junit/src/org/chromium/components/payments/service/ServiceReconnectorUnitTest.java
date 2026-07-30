// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.service;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;

import android.os.Handler;
import android.os.Looper;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Tests for the service reconnector behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class ServiceReconnectorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Reconnectable mConnection;
    @Mock private Handler mHandler;

    @Test
    @Feature({"Payments"})
    public void testTerminateConnectionWhenMaxReconnectsIsZero() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 0, mHandler);

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).terminateConnection();
    }

    @Test
    @Feature({"Payments"})
    public void testUnbindServiceBeforeReconnect() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 1, mHandler);

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).unbindService();
        // First reconnect delay = 1 second.
        Mockito.verify(mHandler).postDelayed(any(), eq(1000L));
    }

    @Test
    @Feature({"Payments"})
    public void testIntentionalDisconnectPreventsReconnects() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 999, mHandler);

        reconnector.onIntentionalServiceDisconnect();

        Mockito.verify(mHandler).removeCallbacksAndMessages(eq(null));

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).terminateConnection();
    }

    @Test
    @Feature({"Payments"})
    public void testThreeReconnectAttempts() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(
                        mConnection, /* maxRetryNumber= */ 3, new Handler(Looper.getMainLooper()));

        reconnector.onUnexpectedServiceDisconnect();

        // First reconnect delay = 1 second.
        // ShadowLooper.idleMainLooper is used to advance the Robolectric test clock.
        // This causes tasks scheduled by the Handler to execute precisely when their delay expires.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
        Mockito.verify(mConnection, Mockito.times(1)).connectToService();

        reconnector.onUnexpectedServiceDisconnect();

        // Second reconnect delay = 2 seconds.
        // Fast-forward 1s first to verify that the connectToService isn't called prematurely.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
        Mockito.verify(mConnection, Mockito.times(1)).connectToService();

        // Fast-forward the remaining 1s to trigger the second reconnect.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
        Mockito.verify(mConnection, Mockito.times(2)).connectToService();

        reconnector.onUnexpectedServiceDisconnect();

        // Third reconnect delay = 4 seconds.
        // Fast-forward 3s first to verify that the connectToService isn't called prematurely.
        ShadowLooper.idleMainLooper(3, TimeUnit.SECONDS);
        Mockito.verify(mConnection, Mockito.times(2)).connectToService();

        // Fast-forward the remaining 1s to trigger the third reconnect.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
        Mockito.verify(mConnection, Mockito.times(3)).connectToService();

        reconnector.onUnexpectedServiceDisconnect();

        // Give up reconnecting after 3 attempts.
        Mockito.verify(mConnection).terminateConnection();
    }

    @Test
    @Feature({"Payments"})
    public void testReconnectDebouncing() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 3, mHandler);

        reconnector.onUnexpectedServiceDisconnect();
        reconnector.onUnexpectedServiceDisconnect();

        // The second call should be ignored, meaning unbindService and postDelayed
        // are only invoked once.
        Mockito.verify(mConnection, Mockito.times(1)).unbindService();
        Mockito.verify(mHandler, Mockito.times(1)).postDelayed(any(), eq(1000L));
    }
}
