// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Tests that ChannelsUpdater correctly initializes channels on the notification manager.
 */
@RunWith(BaseRobolectricTestRunner.class)
@RequiresApi(Build.VERSION_CODES.O)
public class ChannelsUpdaterTest {
    private NotificationManagerProxy mNotificationManagerProxy;
    private SharedPreferencesManager mSharedPreferences;
    private ChannelsInitializer mChannelsInitializer;
    private Resources mMockResources;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);

        Context context = RuntimeEnvironment.getApplication();
        mNotificationManagerProxy = new NotificationManagerProxyImpl(context);

        mMockResources = context.getResources();

        mChannelsInitializer = new ChannelsInitializer(mNotificationManagerProxy,
                ChromeChannelDefinitions.getInstance(), context.getResources());
        mSharedPreferences = SharedPreferencesManager.getInstance();

        // Delete any channels that may already have been initialized. Cleaning up here rather than
        // in tearDown in case tests running before these ones caused channels to be created.
        for (NotificationChannel channel : getChannelsIgnoringDefault()) {
            if (!channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) {
                mNotificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
    }

    @After
    public void tearDown() {
        // TODO(https://crbug.com/1086663): Replace with a SharedPreferencesTestRule when
        //     implemented.
        mSharedPreferences.removeKey(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION);
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testShouldUpdateChannels_returnsFalsePreO() {
        ChannelsUpdater updater = new ChannelsUpdater(
                false /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 0);
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testShouldUpdateChannels_returnsTrueIfOAndNoSavedVersionInPrefs() {
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 0);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testShouldUpdateChannels_returnsTrueIfOAndDifferentVersionInPrefs() {
        mSharedPreferences.writeInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, 4);
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 5);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testShouldUpdateChannels_returnsFalseIfOAndSameVersionInPrefs() {
        mSharedPreferences.writeInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, 3);
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 3);
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testUpdateChannels_noopPreO() {
        ChannelsUpdater updater = new ChannelsUpdater(
                false /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();

        assertThat(getChannelsIgnoringDefault(), hasSize(0));
        assertThat(
                mSharedPreferences.readInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, -1),
                is(-1));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testUpdateChannels_createsExpectedChannelsAndUpdatesPref() {
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();

        assertThat(getChannelsIgnoringDefault(), hasSize((greaterThan(0))));
        assertThat(getChannelIds(getChannelsIgnoringDefault()),
                containsInAnyOrder(ChromeChannelDefinitions.ChannelId.BROWSER,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        ChromeChannelDefinitions.ChannelId.INCOGNITO,
                        ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK));
        assertThat(
                mSharedPreferences.readInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, -1),
                is(21));
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @RequiresApi(Build.VERSION_CODES.O)
    public void testUpdateChannels_deletesLegacyChannelsAndCreatesExpectedOnes() {
        ChromeChannelDefinitions definitions = ChromeChannelDefinitions.getInstance();
        // Set up any legacy channels.
        mNotificationManagerProxy.createNotificationChannelGroup(
                definitions.getChannelGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL)
                        .toNotificationChannelGroup(mMockResources));
        for (String id : definitions.getLegacyChannelIds()) {
            NotificationChannel channel =
                    new NotificationChannel(id, id, NotificationManager.IMPORTANCE_LOW);
            channel.setGroup(ChromeChannelDefinitions.ChannelGroupId.GENERAL);
            mNotificationManagerProxy.createNotificationChannel(channel);
        }

        ChannelsUpdater updater = new ChannelsUpdater(true /* isAtLeastO */, mSharedPreferences,
                new ChannelsInitializer(mNotificationManagerProxy, definitions, mMockResources),
                12);
        updater.updateChannels();

        assertThat(getChannelIds(getChannelsIgnoringDefault()),
                containsInAnyOrder(ChromeChannelDefinitions.ChannelId.BROWSER,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        ChromeChannelDefinitions.ChannelId.INCOGNITO,
                        ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK));
    }

    private static List<String> getChannelIds(List<NotificationChannel> channels) {
        List<String> ids = new ArrayList<>();
        for (NotificationChannel ch : channels) {
            ids.add(ch.getId());
        }
        return ids;
    }

    /**
     * Gets the current notification channels from the notification manager, except for any with
     * the default ID, which will be removed from the list before returning.
     *
     * (Android *might* add a default 'Misc' channel on our behalf, but we don't want to tie our
     * tests to its presence, as this could change).
     */
    private List<NotificationChannel> getChannelsIgnoringDefault() {
        List<NotificationChannel> channels = mNotificationManagerProxy.getNotificationChannels();
        for (Iterator<NotificationChannel> it = channels.iterator(); it.hasNext();) {
            NotificationChannel channel = it.next();
            if (channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) it.remove();
        }
        return channels;
    }
}
