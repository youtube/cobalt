// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.RestrictionsManager;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.OptIn;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.core.os.BuildCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.ui.util.ColorUtils;

import java.util.Locale;

/**
 * A class containing some utility static methods.
 */
public class MediaViewerUtils {
    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIMETYPE_AUDIO = "audio";
    private static final String MIMETYPE_IMAGE = "image";
    private static final String MIMETYPE_VIDEO = "video";

    private static boolean sIsMediaLauncherActivityForceEnabledForTest;

    /**
     * Creates an Intent that allows viewing the given file in an internal media viewer.
     * @param displayUri               URI to display to the user, ideally in file:// form.
     * @param contentUri               content:// URI pointing at the file.
     * @param mimeType                 MIME type of the file.
     * @param allowExternalAppHandlers Whether the viewer should allow the user to open with another
     *                                 app.
     * @return Intent that can be fired to open the file.
     */
    public static Intent getMediaViewerIntent(Uri displayUri, Uri contentUri, String mimeType,
            boolean allowExternalAppHandlers, Context context) {
        Bitmap closeIcon = BitmapFactory.decodeResource(
                context.getResources(), R.drawable.ic_arrow_back_white_24dp);
        Bitmap shareIcon = BitmapFactory.decodeResource(
                context.getResources(), R.drawable.ic_share_white_24dp);

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setToolbarColor(Color.BLACK);
        builder.setCloseButtonIcon(closeIcon);
        builder.setShowTitle(true);
        builder.setColorScheme(ColorUtils.inNightMode(context)
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);

        if (allowExternalAppHandlers && !willExposeFileUri(contentUri)) {
            // Create a PendingIntent that can be used to view the file externally.
            // TODO(https://crbug.com/795968): Check if this is problematic in multi-window mode,
            //                                 where two different viewers could be visible at the
            //                                 same time.
            Intent viewIntent = createViewIntentForUri(contentUri, mimeType, null, null);
            Intent chooserIntent = Intent.createChooser(viewIntent, null);
            chooserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            String openWithStr = context.getString(R.string.download_manager_open_with);

            // TODO(https://crbug.com/1428364): PendingIntents are no longer allowed to be both
            // mutable and implicit. Since this must be mutable, we need to set a component and then
            // remove the FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT flag.
            PendingIntent pendingViewIntent = PendingIntent.getActivity(context, 0, chooserIntent,
                    PendingIntent.FLAG_CANCEL_CURRENT
                            | IntentUtils.getPendingIntentMutabilityFlag(true)
                            | getAllowUnsafeImplicitIntentFlag());
            builder.addMenuItem(openWithStr, pendingViewIntent);
        }

        // Create a PendingIntent that shares the file with external apps.
        // If the URI is a file URI and the Android version is N or later, this will throw a
        // FileUriExposedException. In this case, we just don't add the share button.
        if (!willExposeFileUri(contentUri)) {
            // TODO(https://crbug.com/1428364): PendingIntents are no longer allowed to be both
            // mutable and implicit. Since this must be mutable, we need to set a component and then
            // remove the FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT flag.
            PendingIntent pendingShareIntent =
                    PendingIntent.getActivity(context, 0, createShareIntent(contentUri, mimeType),
                            PendingIntent.FLAG_CANCEL_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(true)
                                    | getAllowUnsafeImplicitIntentFlag());
            builder.setActionButton(
                    shareIcon, context.getString(R.string.share), pendingShareIntent, true);
        }

        // The color of the media viewer is dependent on the file type.
        int backgroundRes;
        if (isImageType(mimeType)) {
            backgroundRes = R.color.image_viewer_bg;
        } else {
            backgroundRes = R.color.media_viewer_bg;
        }
        int mediaColor = context.getColor(backgroundRes);

        // Build up the Intent further.
        Intent intent = builder.build().intent;
        intent.setPackage(context.getPackageName());
        intent.setData(contentUri);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.MEDIA_VIEWER);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_MEDIA_VIEWER_URL, displayUri.toString());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, mediaColor);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, mediaColor);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(context, ChromeLauncherActivity.class);
        return intent;
    }

    /**
     * Creates an Intent to open the file in another app by firing an Intent to Android.
     * @param fileUri  Uri pointing to the file.
     * @param mimeType MIME type for the file.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer Referrer of the downloaded file.
     * @return Intent that can be used to start an Activity for the file.
     */
    public static Intent createViewIntentForUri(
            Uri fileUri, String mimeType, String originalUrl, String referrer) {
        Intent fileIntent = new Intent(Intent.ACTION_VIEW);
        String normalizedMimeType = Intent.normalizeMimeType(mimeType);
        if (TextUtils.isEmpty(normalizedMimeType)) {
            fileIntent.setData(fileUri);
        } else {
            fileIntent.setDataAndType(fileUri, normalizedMimeType);
        }
        fileIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        fileIntent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        fileIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        setOriginalUrlAndReferralExtraToIntent(fileIntent, originalUrl, referrer);
        return fileIntent;
    }

    /**
     * Adds the originating Uri and referrer extras to an intent if they are not null.
     * @param intent      Intent for adding extras.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer    Referrer of the downloaded file.
     */
    public static void setOriginalUrlAndReferralExtraToIntent(
            Intent intent, String originalUrl, String referrer) {
        if (originalUrl != null) {
            intent.putExtra(Intent.EXTRA_ORIGINATING_URI, Uri.parse(originalUrl));
        }
        if (referrer != null) intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrer));
    }

    /**
     * Determines the media type from the given MIME type.
     * @param mimeType The MIME type to check.
     * @return MediaLauncherActivity.MediaType enum value for determined media type.
     */
    static int getMediaTypeFromMIMEType(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return MediaLauncherActivity.MediaType.UNKNOWN;

        String[] pieces = mimeType.toLowerCase(Locale.getDefault()).split("/");
        if (pieces.length != 2) return MediaLauncherActivity.MediaType.UNKNOWN;

        switch (pieces[0]) {
            case MIMETYPE_AUDIO:
                return MediaLauncherActivity.MediaType.AUDIO;
            case MIMETYPE_IMAGE:
                return MediaLauncherActivity.MediaType.IMAGE;
            case MIMETYPE_VIDEO:
                return MediaLauncherActivity.MediaType.VIDEO;
            default:
                return MediaLauncherActivity.MediaType.UNKNOWN;
        }
    }

    /**
     * Selectively enables or disables the MediaLauncherActivity.
     */
    public static void updateMediaLauncherActivityEnabled() {
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> { synchronousUpdateMediaLauncherActivityEnabled(); });
    }

    static void synchronousUpdateMediaLauncherActivityEnabled() {
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ComponentName mediaComponentName = new ComponentName(context, MediaLauncherActivity.class);
        ComponentName audioComponentName = new ComponentName(
                context, "org.chromium.chrome.browser.media.AudioLauncherActivity");

        int newMediaState = shouldEnableMediaLauncherActivity()
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
        int newAudioState = shouldEnableAudioLauncherActivity()
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
        // This indicates that we don't want to kill Chrome when changing component enabled
        // state.
        int flags = PackageManager.DONT_KILL_APP;

        if (packageManager.getComponentEnabledSetting(mediaComponentName) != newMediaState) {
            packageManager.setComponentEnabledSetting(mediaComponentName, newMediaState, flags);
        }
        if (packageManager.getComponentEnabledSetting(audioComponentName) != newAudioState) {
            packageManager.setComponentEnabledSetting(audioComponentName, newAudioState, flags);
        }
    }

    /**
     * Force MediaLauncherActivity to be enabled for testing.
     */
    public static void forceEnableMediaLauncherActivityForTest() {
        sIsMediaLauncherActivityForceEnabledForTest = true;
        // Synchronously update to avoid race conditions in tests.
        synchronousUpdateMediaLauncherActivityEnabled();
    }

    /**
     * Stops forcing MediaLauncherActivity to be enabled for testing.
     */
    public static void stopForcingEnableMediaLauncherActivityForTest() {
        sIsMediaLauncherActivityForceEnabledForTest = false;
        // Synchronously update to avoid race conditions in tests.
        synchronousUpdateMediaLauncherActivityEnabled();
    }

    private static boolean shouldEnableMediaLauncherActivity() {
        return sIsMediaLauncherActivityForceEnabledForTest || SysUtils.isAndroidGo()
                || isEnterpriseManaged();
    }

    private static boolean shouldEnableAudioLauncherActivity() {
        return shouldEnableMediaLauncherActivity() && !SysUtils.isAndroidGo();
    }

    private static boolean isEnterpriseManaged() {

        RestrictionsManager restrictionsManager =
                (RestrictionsManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.RESTRICTIONS_SERVICE);
        return restrictionsManager.hasRestrictionsProvider()
                || !restrictionsManager.getApplicationRestrictions().isEmpty();
    }

    private static Intent createShareIntent(Uri fileUri, String mimeType) {
        if (TextUtils.isEmpty(mimeType)) mimeType = DEFAULT_MIME_TYPE;

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra(Intent.EXTRA_STREAM, fileUri);
        intent.setType(mimeType);
        return intent;
    }

    private static boolean isImageType(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return false;

        String[] pieces = mimeType.toLowerCase(Locale.getDefault()).split("/");
        if (pieces.length != 2) return false;

        return MIMETYPE_IMAGE.equals(pieces[0]);
    }

    private static boolean willExposeFileUri(Uri uri) {
        assert uri != null && !uri.equals(Uri.EMPTY) : "URI is not successfully generated.";
        return uri.getScheme().equals(ContentResolver.SCHEME_FILE);
    }

    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    private static int getAllowUnsafeImplicitIntentFlag() {
        if (BuildCompat.isAtLeastU()) {
            try {
                return PendingIntent.class.getDeclaredField("FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT")
                        .getInt(null);
            } catch (IllegalAccessException | NoSuchFieldException e) {
                assert false : "Unsafe implicit PendingIntent may fail to run.";
            }
        }
        return 0;
    }
}
