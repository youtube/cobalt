// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.os.BuildCompat;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.android_share_sheet.AndroidShareSheetControllerUnitTest.ShadowShareImageFileUtils;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialog;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

/**
 * Test for {@link AndroidShareSheetController} and {@link AndroidCustomActionProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.SHARE_SHEET_CUSTOM_ACTIONS_POLISH})
@Features.DisableFeatures(
        {ChromeFeatureList.WEBNOTES_STYLIZE, ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO})
@Config(shadows = {ShadowShareImageFileUtils.class, ShadowGURL.class})
public class AndroidShareSheetControllerUnitTest {
    private static final String INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS =
            "android.intent.extra.CHOOSER_CUSTOM_ACTIONS";
    private static final String INTENT_EXTRA_CHOOSER_MODIFY_SHARE_ACTION =
            "android.intent.extra.CHOOSER_MODIFY_SHARE_ACTION";
    private static final String KEY_CHOOSER_ACTION_ICON = "icon";
    private static final String KEY_CHOOSER_ACTION_NAME = "name";
    private static final String KEY_CHOOSER_ACTION_ACTION = "action";
    private static final String SELECTOR_FOR_LINK_TO_TEXT = "selector";

    private static final Uri TEST_WEB_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.web.favicon.preview");
    private static final Uri TEST_FALLBACK_FAVICON_PREVIEW_URI =
            Uri.parse("content://test.fallback.favicon.preview");

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    SendTabToSelfAndroidBridgeJni mMockSendTabToSelfAndroidBridge;
    @Mock
    UserPrefsJni mMockUserPrefsJni;
    @Mock
    FaviconHelperJni mMockFaviconHelperJni;
    @Mock
    DomDistillerUrlUtilsJni mMockDomDistillerUrlUtilsJni;
    @Mock
    BottomSheetController mBottomSheetController;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    Tab mTab;
    @Mock
    Profile mProfile;
    @Mock
    Tracker mTracker;

    private TestActivity mActivity;
    private WindowAndroid mWindow;
    private PayloadCallbackHelper<Tab> mPrintCallback;
    private AndroidShareSheetController mController;
    private Bitmap mTestWebFavicon;

    @Before
    public void setup() {
        TrackerFactory.setTrackerForTests(mTracker);

        // Set up send to self option.
        mJniMocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mMockSendTabToSelfAndroidBridge);
        doReturn(0)
                .when(mMockSendTabToSelfAndroidBridge)
                .getEntryPointDisplayReason(any(), anyString());
        // Set up print tab option.
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        PrefService service = mock(PrefService.class);
        doReturn(service).when(mMockUserPrefsJni).get(mProfile);
        doReturn(true).when(service).getBoolean(Pref.PRINTING_ENABLED);
        // Set up favicon helper.
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mMockFaviconHelperJni);
        doReturn(1L).when(mMockFaviconHelperJni).init();
        mTestWebFavicon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        ShadowShareImageFileUtils.sExpectedWebBitmap = mTestWebFavicon;
        setFaviconToFetchForTest(mTestWebFavicon);
        // Set up mMockDomDistillerUrlUtilsJni. Needed for link-to-text sharing.
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mMockDomDistillerUrlUtilsJni);
        doAnswer(invocation -> new GURL(invocation.getArgument(0)))
                .when(mMockDomDistillerUrlUtilsJni)
                .getOriginalUrlFromDistillerUrl(anyString());

        mActivityScenario.getScenario().onActivity((activity) -> mActivity = activity);
        mActivityScenario.getScenario().moveToState(State.RESUMED);
        mWindow = new ActivityWindowAndroid(
                mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        mPrintCallback = new PayloadCallbackHelper<>();
        // Set up mock tab
        doReturn(mWindow).when(mTab).getWindowAndroid();

        mController = new AndroidShareSheetController(mBottomSheetController,
                () -> mTab, () -> mTabModelSelector, () -> mProfile, mPrintCallback::notifyCalled);
    }

    @After
    public void tearDown() {
        ShadowLinkToTextCoordinator.setForceToFail(null);
        ShadowQrCodeDialog.sLastUrl = null;
        mWindow.destroy();
        TrackerFactory.setTrackerForTests(null);
    }

    /**
     * Test whether custom actions are attached to the intent.
     */
    @Test
    @Config(shadows = {ShadowBuildCompatForU.class, ShadowChooserActionHelper.class})
    public void shareWithCustomAction() {
        ShareParams params = new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .setFileContentType("text/plain")
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Custom action is empty.",
                intent.getParcelableArrayExtra(INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS));

        assertCustomActions(intent, R.string.sharing_copy_url, R.string.sharing_long_screenshot,
                R.string.print_share_activity_title, R.string.send_tab_to_self_share_activity_title,
                R.string.qr_code_share_icon_label);
    }

    @Test
    public void shareWithoutCustomAction() {
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showThirdPartyShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNull("Custom action should be empty for 3p only share sheet.",
                intent.getParcelableArrayExtra(INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS));
    }

    @Test
    @Config(shadows = {ShadowBuildCompatForU.class, ShadowChooserActionHelper.class})
    public void choosePrintAction() throws CanceledException {
        CallbackHelper callbackHelper = new CallbackHelper();
        TargetChosenCallback callback = new TargetChosenCallback() {
            @Override
            public void onTargetChosen(ComponentName chosenComponent) {
                callbackHelper.notifyCalled();
            }
            @Override
            public void onCancel() {}
        };

        ShareParams params = new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL)
                                     .setFileContentType("text/plain")
                                     .setCallback(callback)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        AndroidShareSheetController.showShareSheet(params, chromeShareExtras,
                mBottomSheetController,
                () -> mTab, () -> mTabModelSelector, () -> mProfile, mPrintCallback::notifyCalled);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        chooseCustomAction(intent, R.string.print_share_activity_title);
        Assert.assertEquals("Print callback is not called.", 1, mPrintCallback.getCallCount());
        Assert.assertEquals(
                "TargetChosenCallback is not called.", 1, callbackHelper.getCallCount());
    }

    @Test
    public void shareImage() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setFileContentType("image/png")
                                     .setSingleImageUri(testImageUri)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL))
                        .setImageSrcUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL_DOGS))
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent sharingIntent = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals("ImageUri does not match.", JUnitTestGURLs.GOOGLE_URL_DOGS,
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Test
    @Config(shadows = {ShadowChooserActionHelper.class, ShadowBuildCompatForU.class})
    public void shareImageWithCustomActions() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setFileContentType("image/png")
                                     .setSingleImageUri(testImageUri)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL))
                        .setImageSrcUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL_DOGS))
                        .build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(intent, R.string.sharing_copy_image, R.string.sharing_long_screenshot,
                R.string.send_tab_to_self_share_activity_title, R.string.qr_code_share_icon_label);
    }

    @Test
    public void shareTextWithPreviewFavicon() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Sharing.PreparePreviewFaviconDuration");

        ShareParams params = new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL)
                                     .setFileContentType("text/plain")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Preview clip data should not be null.", intent.getClipData());
        Assert.assertEquals("Image preview Uri is null.", TEST_WEB_FAVICON_PREVIEW_URI,
                intent.getClipData().getItemAt(0).getUri());
        watcher.assertExpected();
    }

    @Test
    public void shareTextWithPreviewWithFallbackFavicon() {
        // Assume favicon is not available for this test.
        setFaviconToFetchForTest(null);

        ShareParams params = new ShareParams.Builder(mWindow, "title", JUnitTestGURLs.EXAMPLE_URL)
                                     .setFileContentType("text/plain")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNotNull("Preview clip data should not be null.", intent.getClipData());
        Assert.assertEquals("Image preview Uri is not as expected.",
                TEST_FALLBACK_FAVICON_PREVIEW_URI, intent.getClipData().getItemAt(0).getUri());
    }

    @Test
    public void sharePlainTextDoesNotProvidePreview() {
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setFileContentType("text/plain")
                                     .setText("text")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Assert.assertNull("Preview clip data should be null.", intent.getClipData());
        verifyNoMoreInteractions(mMockFaviconHelperJni);
    }

    @Test
    @Config(shadows = {ShadowLinkToTextCoordinator.class, ShadowBuildCompatForU.class,
                    ShadowChooserActionHelper.class})
    public void
    shareLinkToHighlightText() throws CanceledException {
        ShareParams params = new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL)
                                     .setFileContentType("text/plain")
                                     .setText("highlight")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        AndroidShareSheetController.showShareSheet(params, chromeShareExtras,
                mBottomSheetController,
                () -> mTab, () -> mTabModelSelector, () -> mProfile, mPrintCallback::notifyCalled);

        Intent chooserIntent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent = chooserIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals("Text being shared is different.",
                "\"highlight\"\n " + JUnitTestGURLs.TEXT_FRAGMENT_URL,
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));

        assertCustomActions(chooserIntent, R.string.sharing_copy, R.string.sharing_long_screenshot,
                R.string.send_tab_to_self_share_activity_title, R.string.qr_code_share_icon_label);

        // Toggle the modify action again, link is removed from text.
        runModifyActionFromChooserIntent(chooserIntent);
        Intent chooserIntent2 = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent2 = chooserIntent2.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals("Text being shared is different.", "highlight",
                shareIntent2.getStringExtra(Intent.EXTRA_TEXT));

        assertCustomActions(
                chooserIntent2, R.string.sharing_copy_text, R.string.sharing_long_screenshot);

        // Toggle the modify action again, link is reattached with the text.
        runModifyActionFromChooserIntent(chooserIntent2);
        Intent chooserIntent3 = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent3 = chooserIntent3.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals("Text being shared is different.",
                "\"highlight\"\n " + JUnitTestGURLs.TEXT_FRAGMENT_URL,
                shareIntent3.getStringExtra(Intent.EXTRA_TEXT));
    }

    @Test
    @Config(shadows = {ShadowLinkToTextCoordinator.class, ShadowBuildCompatForU.class,
                    ShadowChooserActionHelper.class})
    public void
    shareLinkToHighlightTextFailed() {
        ShadowLinkToTextCoordinator.setForceToFail(true);

        ShareParams params = new ShareParams.Builder(mWindow, "", JUnitTestGURLs.EXAMPLE_URL)
                                     .setFileContentType("text/plain")
                                     .setText("highlight")
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        AndroidShareSheetController.showShareSheet(params, chromeShareExtras,
                mBottomSheetController,
                () -> mTab, () -> mTabModelSelector, () -> mProfile, mPrintCallback::notifyCalled);

        // Since link to share failed, the content being shared is a plain text.
        Intent chooserIntent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        Intent shareIntent = chooserIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertEquals("Text being shared is different.", "highlight",
                shareIntent.getStringExtra(Intent.EXTRA_TEXT));
        Assert.assertNull("Modify action should be null when generating link to text failed.",
                chooserIntent.getParcelableExtra(INTENT_EXTRA_CHOOSER_MODIFY_SHARE_ACTION));

        assertCustomActions(
                chooserIntent, R.string.sharing_copy_text, R.string.sharing_long_screenshot);
    }

    @Test
    @Config(shadows = {ShadowBuildCompatForU.class, ShadowChooserActionHelper.class,
                    ShadowQrCodeDialog.class})
    public void
    chooseQRCodeAction() throws CanceledException {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setFileContentType("image/png")
                                     .setSingleImageUri(testImageUri)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL))
                        .setImageSrcUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL_DOGS))
                        .build();

        mController.showShareSheet(params, chromeShareExtras, 1L);

        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(intent, R.string.sharing_copy_image, R.string.sharing_long_screenshot,
                R.string.send_tab_to_self_share_activity_title, R.string.qr_code_share_icon_label);
        chooseCustomAction(intent, R.string.qr_code_share_icon_label);

        Assert.assertEquals("Last URL does not match content being shared.",
                JUnitTestGURLs.GOOGLE_URL_DOGS, ShadowQrCodeDialog.sLastUrl);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SHARE_SHEET_CUSTOM_ACTIONS_POLISH)
    @Config(shadows = {ShadowBuildCompatForU.class, ShadowChooserActionHelper.class})
    public void ensureNonPolishActionInOrder() {
        Uri testImageUri = Uri.parse("content://test.image.uri");
        ShareParams params = new ShareParams.Builder(mWindow, "", "")
                                     .setFileContentType("image/png")
                                     .setSingleImageUri(testImageUri)
                                     .setBypassFixingDomDistillerUrl(true)
                                     .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL))
                        .setImageSrcUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL_DOGS))
                        .build();

        mController.showShareSheet(params, chromeShareExtras, 1L);

        // No download option here.
        Intent intent = Shadows.shadowOf((Activity) mActivity).peekNextStartedActivity();
        assertCustomActions(intent, R.string.sharing_long_screenshot, R.string.sharing_copy_image,
                R.string.send_tab_to_self_share_activity_title, R.string.qr_code_share_icon_label);
    }

    private void setFaviconToFetchForTest(Bitmap favicon) {
        doAnswer(invocation -> {
            FaviconHelper.FaviconImageCallback callback = invocation.getArgument(4);
            callback.onFaviconAvailable(favicon, GURL.emptyGURL());
            return null;
        })
                .when(mMockFaviconHelperJni)
                .getLocalFaviconImageForURL(anyLong(), eq(mProfile), any(), anyInt(), any());
    }

    private void runModifyActionFromChooserIntent(Intent chooserIntent) throws CanceledException {
        Bundle modifyAction =
                chooserIntent.getParcelableExtra(INTENT_EXTRA_CHOOSER_MODIFY_SHARE_ACTION);
        PendingIntent action = modifyAction.getParcelable(KEY_CHOOSER_ACTION_ACTION);
        action.send();
        ShadowLooper.idleMainLooper();
    }

    private void assertCustomActions(Intent chooserIntent, Integer... expectedStringRes) {
        Parcelable[] actions =
                chooserIntent.getParcelableArrayExtra(INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS);
        StringBuilder actualStringBuilder = new StringBuilder();
        for (Parcelable action : actions) {
            String name = ((Bundle) action).getString(KEY_CHOOSER_ACTION_NAME);
            actualStringBuilder.append(",").append(name);
        }

        StringBuilder expectedStringBuilder = new StringBuilder();
        for (int stringRes : expectedStringRes) {
            String name = ContextUtils.getApplicationContext().getString(stringRes);
            expectedStringBuilder.append(",").append(name);
        }

        String actualString = actualStringBuilder.toString();
        String expectedString = expectedStringBuilder.toString();
        Assert.assertEquals(
                "Actions and/or the order does not match.", expectedString, actualString);
    }

    private void chooseCustomAction(Intent chooserIntent, @StringRes int iconLabel)
            throws CanceledException {
        Parcelable[] actions =
                chooserIntent.getParcelableArrayExtra(INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS);
        Assert.assertTrue("More than one action is provided.", actions.length > 0);

        // Find the print callback, since we mocked that out during this test.
        Bundle expectAction = null;
        for (Parcelable parcelable : actions) {
            Bundle bundle = (Bundle) parcelable;
            if (TextUtils.equals(
                        ContextUtils.getApplicationContext().getResources().getString(iconLabel),
                        bundle.getString(KEY_CHOOSER_ACTION_NAME))) {
                expectAction = bundle;
                break;
            }
        }

        Assert.assertNotNull("Print option is null when the callback is provided.", expectAction);

        PendingIntent action = expectAction.getParcelable(KEY_CHOOSER_ACTION_ACTION);
        action.send();
        ShadowLooper.idleMainLooper();
    }

    /**
     * Test implementation to build a ChooserAction.
     */
    @Implements(ShareHelper.ChooserActionHelper.class)
    static class ShadowChooserActionHelper {
        @Implementation
        protected static boolean isSupported() {
            return true;
        }

        @Implementation
        protected static Parcelable newChooserAction(Icon icon, String name, PendingIntent action) {
            Bundle bundle = new Bundle();
            bundle.putParcelable(KEY_CHOOSER_ACTION_ICON, icon);
            bundle.putString(KEY_CHOOSER_ACTION_NAME, name);
            bundle.putParcelable(KEY_CHOOSER_ACTION_ACTION, action);
            return bundle;
        }
    }

    // Work around shadow to assume runtime is at least U.
    // TODO(https://crbug.com/1420388): Switch to @Config(sdk=34) this once API 34 exists.
    @Implements(BuildCompat.class)
    static class ShadowBuildCompatForU {
        @Implementation
        protected static boolean isAtLeastU() {
            return true;
        }
    }

    // Shadow class to bypass actually saving the image as URL for this test.
    @Implements(ShareImageFileUtils.class)
    static class ShadowShareImageFileUtils {
        static @Nullable Bitmap sExpectedWebBitmap;

        @Implementation
        public static void generateTemporaryUriFromBitmap(
                String fileName, Bitmap bitmap, Callback<Uri> callback) {
            if (bitmap != null && bitmap.equals(sExpectedWebBitmap)) {
                callback.onResult(TEST_WEB_FAVICON_PREVIEW_URI);
            } else {
                callback.onResult(TEST_FALLBACK_FAVICON_PREVIEW_URI);
            }
        }
    }

    /**
     * Shadow implementation of the real LinkToTextCoordinator but bypassing the selector process.
     */
    @Implements(LinkToTextCoordinator.class)
    public static class ShadowLinkToTextCoordinator {
        @RealObject
        private LinkToTextCoordinator mRealObj;

        public ShadowLinkToTextCoordinator() {}

        static Boolean sForceToFail;

        static void setForceToFail(Boolean forceToFail) {
            sForceToFail = forceToFail;
        }

        @Implementation
        protected void shareLinkToText() {
            boolean fail = sForceToFail != null && sForceToFail;
            mRealObj.onSelectorReady(fail ? "" : SELECTOR_FOR_LINK_TO_TEXT);
        }
    }

    @Implements(QrCodeDialog.class)
    static class ShadowQrCodeDialog {
        static @Nullable String sLastUrl;

        @Implementation
        protected static QrCodeDialog newInstance(String url, WindowAndroid windowAndroid) {
            sLastUrl = url;
            return Mockito.mock(QrCodeDialog.class);
        }
    }
}
