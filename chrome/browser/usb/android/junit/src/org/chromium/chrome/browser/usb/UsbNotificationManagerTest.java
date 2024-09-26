// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.usb;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.List;

/**
 * Tests UsbNotificationManager behaviour and its delegate.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowGURL.class)
public class UsbNotificationManagerTest {
    private static final int NOTIFICATION_ID = 0;
    private static final GURL TEST_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    private MockNotificationManagerProxy mMockNotificationManager;
    private UsbNotificationManagerDelegate mDelegate = new UsbNotificationManagerDelegate() {
        @Override
        public Intent createTrustedBringTabToFrontIntent(int tabId) {
            mTabsBroughtToFront++;
            return new Intent();
        }
        @Override
        public void stopSelf() {
            mServiceStopped = true;
        }
        @Override
        public void stopSelf(int startId) {
            mServiceStopped = true;
            mLastStartId = startId;
        }
    };

    private class FakeService {};
    private UsbNotificationManager mManager;
    private boolean mServiceStopped;
    private int mLastStartId;
    private int mTabsBroughtToFront;

    private Intent createIntent(boolean isConnected) {
        return createIntent(isConnected, /*isIncognito=*/false);
    }

    private Intent createIntent(boolean isConnected, boolean isIncognito) {
        Intent intent = new Intent(mock(Context.class), FakeService.class);
        intent.setAction(UsbNotificationManager.ACTION_USB_UPDATE);
        intent.putExtra(UsbNotificationManager.NOTIFICATION_ID_EXTRA, NOTIFICATION_ID);
        intent.putExtra(UsbNotificationManager.NOTIFICATION_URL_EXTRA, TEST_URL.getSpec());
        intent.putExtra(UsbNotificationManager.NOTIFICATION_IS_CONNECTED, isConnected);
        intent.putExtra(UsbNotificationManager.NOTIFICATION_IS_INCOGNITO, isIncognito);
        return intent;
    }

    private void assertNotificationEquals(String expectedTitle, String expectedText) {
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(expectedTitle,
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(expectedText,
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(
                     any(GURL.class), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> ((GURL) (inv.getArgument(0))).getSpec());

        mMockNotificationManager = new MockNotificationManagerProxy();
        mManager = new UsbNotificationManager(mMockNotificationManager, mDelegate);
    }

    @Test
    public void test_nullIntentStopsService() {
        mServiceStopped = false;
        mManager.onStartCommand(null, 0, /*startId=*/0);
        assertTrue(mServiceStopped);
        assertEquals(0, mLastStartId);
        assertEquals(0, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_nullIntentWithStartIdStopsService() {
        mServiceStopped = false;
        mManager.onStartCommand(null, 0, /*startId=*/42);
        assertTrue(mServiceStopped);
        assertEquals(0, mLastStartId);
        assertEquals(0, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_connectedShowsNotification() {
        Intent intent = createIntent(/*isConnected=*/true);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals(
                "Connected to a USB device", "Tap to return to https://www.example.com/");
    }

    @Test
    public void test_disconnectHidesNotification() {
        Intent intent1 = createIntent(/*isConnected=*/true);
        mManager.onStartCommand(intent1, 0, /*startId=*/42);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertEquals(0, mLastStartId);
        assertNotificationEquals(
                "Connected to a USB device", "Tap to return to https://www.example.com/");

        Intent intent2 = createIntent(/*isConnected=*/false);
        mManager.onStartCommand(intent2, 0, /*startId=*/42);

        assertTrue(mServiceStopped);
        assertEquals(42, mLastStartId);
        assertEquals(1, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_connectedInIncognitoShowsNotification() {
        Intent intent = createIntent(/*isConnected=*/true, /*isIncognito=*/true);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals("Connected to a USB device", "Tap to return to the site");
    }
}
