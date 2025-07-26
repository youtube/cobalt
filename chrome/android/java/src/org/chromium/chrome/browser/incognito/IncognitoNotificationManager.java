// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Notification;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** Manages the notification indicating that there are incognito tabs opened in Document mode. */
public class IncognitoNotificationManager {
    public static final String INCOGNITO_TABS_OPEN_TAG = "incognito_tabs_open";
    private static final int INCOGNITO_TABS_OPEN_ID = 100;

    /** Shows the close all incognito notification. */
    public static void showIncognitoNotification() {
        Context context = ContextUtils.getApplicationContext();
        String actionMessage = context.getString(R.string.close_all_incognito_notification);

        // From Android N, notification by default has the app name and title should not be the same
        // as app name.
        String title = context.getString(R.string.close_all_incognito_notification_title);

        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.INCOGNITO,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .CLOSE_INCOGNITO,
                                        INCOGNITO_TABS_OPEN_TAG,
                                        INCOGNITO_TABS_OPEN_ID))
                        .setContentTitle(title)
                        .setContentIntent(
                                IncognitoNotificationServiceImpl.getRemoveAllIncognitoTabsIntent(
                                        context))
                        .setContentText(actionMessage)
                        .setOngoing(true)
                        .setVisibility(Notification.VISIBILITY_SECRET)
                        .setSmallIcon(R.drawable.incognito_simple)
                        .setShowWhen(false)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_INCOGNITO);
        NotificationWrapper notification = builder.buildNotificationWrapper();
        BaseNotificationManagerProxyFactory.create().notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.CLOSE_INCOGNITO,
                        notification.getNotification());
    }

    /** Dismisses the incognito notification. */
    public static void dismissIncognitoNotification() {
        BaseNotificationManagerProxyFactory.create()
                .cancel(INCOGNITO_TABS_OPEN_TAG, INCOGNITO_TABS_OPEN_ID);
    }
}
