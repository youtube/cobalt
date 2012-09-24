// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.telephony.TelephonyManager;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;

public class NetworkChangeNotifierTest extends InstrumentationTestCase {
    /**
     * Listens for alerts fired by the NetworkChangeNotifier when network status changes.
     */
    private static class NetworkChangeNotifierTestObserver
            implements NetworkChangeNotifier.ConnectionTypeObserver {
        private boolean mReceivedNotification = false;

        public void onConnectionTypeChanged(int connectionType) {
            mReceivedNotification = true;
        }

        public boolean hasReceivedNotification() {
            return mReceivedNotification;
        }
    }

    /**
     * Mocks out calls to the ConnectivityManager.
     */
    class MockConnectivityManagerDelegate
            extends NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate {
        private boolean mActiveNetworkExists;
        private int mNetworkType;
        private int mNetworkSubtype;

        MockConnectivityManagerDelegate() {
            super(null);
        }

        @Override
        boolean activeNetworkExists() {
            return mActiveNetworkExists;
        }

        void setActiveNetworkExists(boolean networkExists) {
            mActiveNetworkExists = networkExists;
        }

        @Override
        int getNetworkType() {
            return mNetworkType;
        }

        void setNetworkType(int networkType) {
            mNetworkType = networkType;
        }

        @Override
        int getNetworkSubtype() {
            return mNetworkSubtype;
        }

        void setNetworkSubtype(int networkSubtype) {
            mNetworkSubtype = networkSubtype;
        }
    }

    /**
     * Tests that when Chrome gets an intent indicating a change in network connectivity, it sends a
     * notification to Java observers.
     */
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierJavaObservers() throws InterruptedException {
        // Create a new notifier that doesn't have a native-side counterpart.
        Context context = getInstrumentation().getTargetContext();
        NetworkChangeNotifier.createInstance(context, 0);

        NetworkChangeNotifier.setAutoDetectConnectivityState(true);
        NetworkChangeNotifierAutoDetect receiver = NetworkChangeNotifier.getAutoDetectorForTest();
        assertTrue(receiver != null);

        MockConnectivityManagerDelegate connectivityDelegate =
                new MockConnectivityManagerDelegate();
        connectivityDelegate.setActiveNetworkExists(true);
        connectivityDelegate.setNetworkType(NetworkChangeNotifier.CONNECTION_UNKNOWN);
        connectivityDelegate.setNetworkSubtype(TelephonyManager.NETWORK_TYPE_UNKNOWN);
        receiver.setConnectivityManagerDelegateForTests(connectivityDelegate);

        // Initialize the NetworkChangeNotifier with a connection.
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);

        // We shouldn't be re-notified if the connection hasn't actually changed.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertFalse(observer.hasReceivedNotification());

        // Mimic that connectivity has been lost and ensure that Chrome notifies our observer.
        connectivityDelegate.setActiveNetworkExists(false);
        connectivityDelegate.setNetworkType(NetworkChangeNotifier.CONNECTION_NONE);
        Intent noConnectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        noConnectivityIntent.putExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, true);
        receiver.onReceive(getInstrumentation().getTargetContext(), noConnectivityIntent);
        assertTrue(observer.hasReceivedNotification());
    }
}
