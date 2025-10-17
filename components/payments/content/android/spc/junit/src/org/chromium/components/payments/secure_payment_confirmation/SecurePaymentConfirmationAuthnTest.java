// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.R;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationAuthnController.SpcResponseStatus;
import org.chromium.components.payments.test_support.DefaultPaymentFeatureConfig;
import org.chromium.components.payments.ui.CurrencyFormatter;
import org.chromium.components.payments.ui.CurrencyFormatterJni;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.payments.ui.test_support.FakeClock;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;

/** A test for SecurePaymentConfirmationAuthn. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {SecurePaymentConfirmationAuthnTest.ShadowBottomSheetControllerProvider.class})
@EnableFeatures({
    BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_NETWORK_AND_ISSUER_ICONS,
    PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_FALLBACK
})
@DisableFeatures(PaymentFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES)
public class SecurePaymentConfirmationAuthnTest {
    private static final long IGNORED_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100;
    private static final long SAFE_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    private boolean mIsPaymentOptOut;
    private Callback<Integer> mResponseCallback;
    private Runnable mOptOutCallback;

    private String mPayeeName;
    private Origin mPayeeOrigin;
    private PaymentItem mTotal;
    private Drawable mPaymentIcon;
    private Drawable mIssuerIcon;
    private Drawable mNetworkIcon;
    private SecurePaymentConfirmationAuthnController mAuthnController;
    private final FakeClock mClock = new FakeClock();
    private @SpcResponseStatus int mResponseStatus = SpcResponseStatus.UNKNOWN;

    /** The shadow of BottomSheetControllerProvider. Not to use outside the test. */
    @Implements(BottomSheetControllerProvider.class)
    /* package */ static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        private static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    @Before
    public void setUp() {
        DefaultPaymentFeatureConfig.setDefaultFlagConfigurationForTesting();

        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        setWindowAndroid(windowAndroid, mWebContents);
        Mockito.doReturn(new WeakReference<Context>(RuntimeEnvironment.application))
                .when(windowAndroid)
                .getContext();

        CurrencyFormatter.Natives currencyFormatterJniMock =
                Mockito.mock(CurrencyFormatter.Natives.class);
        CurrencyFormatterJni.setInstanceForTesting(currencyFormatterJniMock);
        Mockito.doReturn("$1.00")
                .when(currencyFormatterJniMock)
                .format(
                        Mockito.anyLong(),
                        Mockito.any(CurrencyFormatter.class),
                        Mockito.anyString());

        mPayeeName = "My Store";
        mPayeeOrigin = Origin.create(new GURL("https://store.example:443"));
        mTotal = new PaymentItem();
        mTotal.amount = new PaymentCurrencyAmount();
        mTotal.amount.currency = "USD";
        mTotal.amount.value = "1.00";
        // Our credit card 'icon' is just a red square.
        mPaymentIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.application.getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.RED},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mIssuerIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.getApplication().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.GREEN},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mNetworkIcon =
                new BitmapDrawable(
                        RuntimeEnvironment.getApplication().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.BLUE},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888));
        mResponseCallback =
                (responseStatus) -> {
                    mResponseStatus = responseStatus;
                };
        mOptOutCallback =
                () -> {
                    mIsPaymentOptOut = true;
                };

        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/* requestShowContentResponse= */ true));
    }

    @After
    public void tearDown() {
        if (mAuthnController != null) mAuthnController.hide();
    }

    private void createAuthnController() {
        mAuthnController = SecurePaymentConfirmationAuthnController.create(mWebContents);
        // Some tests expect a null controller, e.g. for a null web contents.
        if (mAuthnController != null) {
            mAuthnController.setInputProtectorForTesting(new InputProtector(mClock));
        }
    }

    private BottomSheetController createBottomSheetController(boolean requestShowContentResponse) {
        BottomSheetController controller = Mockito.mock(BottomSheetController.class);
        Mockito.doReturn(requestShowContentResponse)
                .when(controller)
                .requestShowContent(Mockito.any(BottomSheetContent.class), Mockito.anyBoolean());
        return controller;
    }

    private boolean show() {
        return show(mPayeeName, mPayeeOrigin, /* enableOptOut= */ false, /* informOnly= */ false);
    }

    private boolean showWithPayeeName() {
        return show(mPayeeName, null, /* enableOptOut= */ false, /* informOnly= */ false);
    }

    private boolean showWithPayeeOrigin() {
        return show(null, mPayeeOrigin, /* enableOptOut= */ false, /* informOnly= */ false);
    }

    private boolean showWithOptOut() {
        return show(mPayeeName, mPayeeOrigin, /* enableOptOut= */ true, /* informOnly= */ false);
    }

    private boolean showInformOnly() {
        return show(mPayeeName, mPayeeOrigin, /* enableOptOut= */ false, /* informOnly= */ true);
    }

    private boolean show(
            String payeeName, Origin payeeOrigin, boolean enableOptOut, boolean informOnly) {
        if (mAuthnController == null) return false;

        mIsPaymentOptOut = false;

        String paymentInstrumentLabel = "My Card";
        String rpId = "rp.example";
        return mAuthnController.show(
                mPaymentIcon,
                paymentInstrumentLabel,
                mTotal,
                mResponseCallback,
                mOptOutCallback,
                payeeName,
                payeeOrigin,
                enableOptOut,
                rpId,
                mIssuerIcon,
                mNetworkIcon,
                informOnly);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid, WebContents webContents) {
        Mockito.doReturn(windowAndroid).when(webContents).getTopLevelNativeWindow();
    }

    private void setContext(Context context) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        Mockito.doReturn(new WeakReference<Context>(context)).when(windowAndroid).getContext();
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnConfirmation() {
        createAuthnController();
        show();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        mAuthnController.getView().mContinueButton.performClick();

        Assert.assertEquals(SpcResponseStatus.ACCEPT, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnCancellation() {
        createAuthnController();
        show();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        mAuthnController.getView().mCancelButton.performClick();

        Assert.assertEquals(SpcResponseStatus.CANCEL, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    @DisableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_FALLBACK)
    public void testOnAuthnCancellationFeatureDisabled() {
        createAuthnController();
        show();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        mAuthnController.getView().mCancelButton.performClick();

        Assert.assertEquals(SpcResponseStatus.ANOTHER_WAY, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnOptOut() {
        createAuthnController();
        showWithOptOut();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        SecurePaymentConfirmationAuthnView authnView = mAuthnController.getView();
        authnView.mOptOutText.getClickableSpans()[0].onClick(authnView.mOptOutText);

        Assert.assertTrue(mIsPaymentOptOut);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnAuthnUnintentedInput() {
        createAuthnController();
        showWithOptOut();

        // Clicking immediately is prevented.
        mAuthnController.getView().mContinueButton.performClick();
        Assert.assertEquals(SpcResponseStatus.UNKNOWN, mResponseStatus);
        Assert.assertFalse(mAuthnController.isHidden());

        mAuthnController.getView().mCancelButton.performClick();
        Assert.assertEquals(SpcResponseStatus.UNKNOWN, mResponseStatus);
        Assert.assertFalse(mAuthnController.isHidden());

        SecurePaymentConfirmationAuthnView authnView = mAuthnController.getView();
        authnView.mOptOutText.getClickableSpans()[0].onClick(authnView.mOptOutText);
        Assert.assertFalse(mIsPaymentOptOut);
        Assert.assertFalse(mAuthnController.isHidden());

        // Clicking after an interval less than the threshold is still prevented.
        mClock.advanceCurrentTimeMillis(IGNORED_INPUT_DELAY);

        mAuthnController.getView().mContinueButton.performClick();
        Assert.assertEquals(SpcResponseStatus.UNKNOWN, mResponseStatus);
        Assert.assertFalse(mAuthnController.isHidden());

        mAuthnController.getView().mCancelButton.performClick();
        Assert.assertEquals(SpcResponseStatus.UNKNOWN, mResponseStatus);
        Assert.assertFalse(mAuthnController.isHidden());

        authnView.mOptOutText.getClickableSpans()[0].onClick(authnView.mOptOutText);
        Assert.assertFalse(mIsPaymentOptOut);
        Assert.assertFalse(mAuthnController.isHidden());

        // Clicking confirm after the threshold is no longer prevented and confirms the dialog.
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        mAuthnController.getView().mContinueButton.performClick();
        Assert.assertEquals(SpcResponseStatus.ACCEPT, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnVerifyAnotherWay() {
        createAuthnController();
        show();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        SecurePaymentConfirmationAuthnView authnView = mAuthnController.getView();
        Assert.assertEquals(View.VISIBLE, authnView.mFootnote.getVisibility());
        authnView.mFootnote.getClickableSpans()[0].onClick(authnView.mFootnote);

        Assert.assertEquals(SpcResponseStatus.ANOTHER_WAY, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    @DisableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_FALLBACK)
    public void testOnVerifyAnotherWayFeatureDisabled() {
        createAuthnController();

        show();
        SecurePaymentConfirmationAuthnView authnView = mAuthnController.getView();

        Assert.assertEquals(View.GONE, authnView.mFootnote.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testHide() {
        createAuthnController();
        show();
        mAuthnController.hide();
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testRequestShowContentFalse() {
        createAuthnController();
        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/* requestShowContentResponse= */ false));
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testCreateWithNullWebContents() {
        mWebContents = null;
        createAuthnController();
        Assert.assertNull(mAuthnController);
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullWindowAndroid() {
        setWindowAndroid(null, mWebContents);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullContext() {
        setContext(null);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullBottomSheetController() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);
        createAuthnController();
        Assert.assertFalse(show());
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithHide() {
        createAuthnController();
        Assert.assertTrue(show());
        mAuthnController.hide();
        Assert.assertTrue(show());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithoutHide() {
        createAuthnController();
        Assert.assertTrue(show());
        Assert.assertFalse(show());
    }

    @Test
    @Feature({"Payments"})
    public void testShow() {
        createAuthnController();
        show();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        Assert.assertEquals("My Store (store.example)", view.mStoreLabel.getText());
        Assert.assertEquals("My Card", view.mPaymentInstrumentLabel.getText());
        Assert.assertEquals(mPaymentIcon, view.mPaymentIcon.getDrawable());
        Assert.assertEquals(mNetworkIcon, view.mNetworkIcon.getDrawable());
        Assert.assertEquals(mIssuerIcon, view.mIssuerIcon.getDrawable());
        Assert.assertEquals("$1.00", view.mTotal.getText());
        Assert.assertEquals("USD", view.mCurrency.getText());
        // By default the opt-out text should not be visible.
        Assert.assertEquals(View.GONE, view.mOptOutText.getVisibility());
        // The payment and network icon row should be visible.
        Assert.assertEquals(View.VISIBLE, view.mIssuerNetworkIconsRow.getVisibility());
        // The header image should not be visible.
        Assert.assertEquals(View.GONE, view.mHeaderImage.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    @DisableFeatures(BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_NETWORK_AND_ISSUER_ICONS)
    public void testShowWhenSpcNetworkIssuerIconsFeatureIsDisabled() {
        createAuthnController();
        show();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        Assert.assertEquals("My Store (store.example)", view.mStoreLabel.getText());
        Assert.assertEquals("My Card", view.mPaymentInstrumentLabel.getText());
        Assert.assertEquals(mPaymentIcon, view.mPaymentIcon.getDrawable());
        Assert.assertEquals("$1.00", view.mTotal.getText());
        Assert.assertEquals("USD", view.mCurrency.getText());
        // By default the opt-out text should not be visible.
        Assert.assertEquals(View.GONE, view.mOptOutText.getVisibility());
        // The header image should be visible.
        Assert.assertEquals(View.VISIBLE, view.mHeaderImage.getVisibility());
        // The payment and network icon row should not be visible.
        Assert.assertEquals(View.GONE, view.mIssuerNetworkIconsRow.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWhenNetworkIssuerIconsAreNull() {
        createAuthnController();
        WeakReference<Context> context = mWebContents.getTopLevelNativeWindow().getContext();
        Assert.assertNotNull(context.get());
        mIssuerIcon = null;
        mNetworkIcon = null;

        show();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        // The header image should be visible.
        Assert.assertEquals(View.VISIBLE, view.mHeaderImage.getVisibility());
        // The payment and network icon row should not be visible.
        Assert.assertEquals(View.GONE, view.mIssuerNetworkIconsRow.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWhenIssuerIconIsNull() {
        createAuthnController();
        WeakReference<Context> context = mWebContents.getTopLevelNativeWindow().getContext();
        Assert.assertNotNull(context.get());
        mIssuerIcon = null;

        show();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        // The header image should be visible.
        Assert.assertEquals(View.VISIBLE, view.mHeaderImage.getVisibility());
        // The payment and network icon row should not be visible.
        Assert.assertEquals(View.GONE, view.mIssuerNetworkIconsRow.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWhenNetworkIconIsNull() {
        createAuthnController();
        WeakReference<Context> context = mWebContents.getTopLevelNativeWindow().getContext();
        Assert.assertNotNull(context.get());
        mNetworkIcon = null;

        show();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        // The header image should be visible.
        Assert.assertEquals(View.VISIBLE, view.mHeaderImage.getVisibility());
        // The payment and network icon row should not be visible.
        Assert.assertEquals(View.GONE, view.mIssuerNetworkIconsRow.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testShowAllowsForEmptyPayeeNameOrOrigin() {
        createAuthnController();

        showWithPayeeName();
        Assert.assertEquals("My Store", mAuthnController.getView().mStoreLabel.getText());
        mAuthnController.hide();

        showWithPayeeOrigin();
        Assert.assertEquals("store.example", mAuthnController.getView().mStoreLabel.getText());
    }

    @Test
    @Feature({"Payments"})
    public void testShowHandlesNullIcon() {
        createAuthnController();

        // First validate that our payment icon is used in a normal flow.
        show();
        Assert.assertEquals(mPaymentIcon, mAuthnController.getView().mPaymentIcon.getDrawable());
        mAuthnController.hide();

        // Then make sure it is replaced if we pass in a null bitmap.
        mPaymentIcon = new BitmapDrawable();
        show();
        Assert.assertNotEquals(mPaymentIcon, mAuthnController.getView().mPaymentIcon.getDrawable());
    }

    @Test
    @Feature({"Payments"})
    public void testShowRendersOptOutWhenRequested() {
        createAuthnController();
        showWithOptOut();

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        Assert.assertEquals(View.VISIBLE, view.mOptOutText.getVisibility());
        Assert.assertTrue(view.mOptOutText.getText().toString().contains("rp.example"));
    }

    @Test
    @Feature({"Payments"})
    public void testShowInformOnly() {
        createAuthnController();

        Assert.assertTrue(showInformOnly());

        SecurePaymentConfirmationAuthnView view = mAuthnController.getView();
        Assert.assertNotNull(view);
        Context context = mWebContents.getTopLevelNativeWindow().getContext().get();
        Assert.assertEquals(
                view.mTitle.getText(),
                context.getString(R.string.secure_payment_confirmation_inform_only_title));
        Assert.assertEquals(
                view.mContinueButton.getText(),
                context.getString(R.string.payments_confirm_button));
        Assert.assertEquals(View.GONE, view.mFootnote.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    @DisableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_FALLBACK)
    public void testShowInformOnlyFeatureIsDisabled() {
        createAuthnController();
        Assert.assertThrows(AssertionError.class, this::showInformOnly);
    }

    @Test
    @Feature({"Payments"})
    public void testShowInformOnlyOnConfirmation() {
        createAuthnController();
        showInformOnly();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        mAuthnController.getView().mContinueButton.performClick();

        Assert.assertEquals(SpcResponseStatus.ANOTHER_WAY, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowInformOnlyOnCancellation() {
        createAuthnController();
        showInformOnly();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);

        mAuthnController.getView().mCancelButton.performClick();

        Assert.assertEquals(SpcResponseStatus.CANCEL, mResponseStatus);
        Assert.assertTrue(mAuthnController.isHidden());
    }
}
