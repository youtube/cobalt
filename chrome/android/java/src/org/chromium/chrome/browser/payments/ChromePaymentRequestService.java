// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.AndroidIntentLauncher;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.DialogController;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.components.payments.PaymentResponseHelper;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationAuthnController;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationNoMatchingCredController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.List;
import java.util.Map;

/**
 * This is the Clank specific parts of {@link PaymentRequest}, with the parts shared with WebView
 * living in {@link PaymentRequestService}.
 */
public class ChromePaymentRequestService
        implements BrowserPaymentRequest, PaymentUiService.Delegate {
    // Null-check is necessary because retainers of ChromePaymentRequestService could still
    // reference ChromePaymentRequestService after mPaymentRequestService is set null, e.g.,
    // crbug.com/1122148.
    @Nullable private PaymentRequestService mPaymentRequestService;

    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final JourneyLogger mJourneyLogger;

    private final PaymentUiService mPaymentUiService;
    private final DialogController mDialogController;
    private final AndroidIntentLauncher mAndroidIntentLauncher;

    private boolean mWasRetryCalled;
    private boolean mHasClosed;

    private PaymentRequestSpec mSpec;
    private PaymentHandlerHost mPaymentHandlerHost;

    @Nullable private byte[][] mCertificateChain;

    /**
     * True if the browser has skipped showing the app selector UI (PaymentRequest UI).
     *
     * <p>In cases where there is a single payment app and the merchant does not request shipping or
     * billing, the browser can skip showing UI as the app selector UI is not benefiting the user at
     * all.
     */
    private boolean mHasSkippedAppSelector;

    // mSpcAuthnUiController is null when it is closed and before it is shown.
    @Nullable private SecurePaymentConfirmationAuthnController mSpcAuthnUiController;

    // mNoMatchingController is null when it is closed and before it is shown.
    @Nullable private SecurePaymentConfirmationNoMatchingCredController mNoMatchingController;

    /** The delegate of this class */
    public interface Delegate extends PaymentRequestService.Delegate {
        /**
         * Create PaymentUiService.
         * @param delegate The delegate of this instance.
         * @param webContents The WebContents of the merchant page.
         * @param isOffTheRecord Whether merchant page is in an isOffTheRecord tab.
         * @param journeyLogger The logger of the user journey.
         * @param topLevelOrigin The last committed url of webContents.
         */
        default PaymentUiService createPaymentUiService(
                PaymentUiService.Delegate delegate,
                PaymentRequestParams params,
                WebContents webContents,
                boolean isOffTheRecord,
                JourneyLogger journeyLogger,
                String topLevelOrigin) {
            return new PaymentUiService(
                    /* delegate= */ delegate,
                    /* params= */ params,
                    webContents,
                    isOffTheRecord,
                    journeyLogger,
                    topLevelOrigin);
        }

        /**
         * Looks up the Android Activity of the given web contents. This can be null. Should never
         * be cached, because web contents can change activities, e.g., when user selects "Open in
         * Chrome" menu item.
         *
         * @param webContents The web contents for which to lookup the Android activity.
         * @return Possibly null Android activity that should never be cached.
         */
        default @Nullable Activity getActivity(WebContents webContents) {
            return ChromeActivity.fromWebContents(webContents);
        }

        /**
         * @param renderFrameHost The frame that issues the payment request.
         * @return Whether the WebContents of the merchant frame is alive and visible.
         */
        default boolean isWebContentsActive(RenderFrameHost renderFrameHost) {
            return PaymentRequestServiceUtil.isWebContentsActive(renderFrameHost);
        }

        /**
         * Creates an instance of PaymentHandlerHost.
         * @param webContents The WebContents that issues the payment request.
         * @param listener The listener to payment method, shipping address, and shipping option
         *        change events
         * @return The instance.
         */
        default PaymentHandlerHost createPaymentHandlerHost(
                WebContents webContents, PaymentRequestUpdateEventListener listener) {
            return new PaymentHandlerHost(webContents, listener);
        }

        /**
         * @param webContents Any WebContents.
         * @return The TabModelSelector of the given WebContents.
         */
        default @Nullable TabModelSelector getTabModelSelector(WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getTabModelSelector();
        }

        /**
         * @param webContents Any WebContents.
         * @return The TabModel of the given WebContents.
         */
        default @Nullable TabModel getTabModel(WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getCurrentTabModel();
        }

        /**
         * @param webContents Any WebContents.
         * @return The ActivityLifecycleDispatcher of the ChromeActivity that contains the given
         *         WebContents.
         */
        default @Nullable ActivityLifecycleDispatcher getActivityLifecycleDispatcher(
                WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getLifecycleDispatcher();
        }
    }

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param paymentRequestService The component side of the PaymentRequest implementation.
     * @param delegate The delegate of this class.
     */
    public ChromePaymentRequestService(
            PaymentRequestService paymentRequestService, Delegate delegate) {
        assert paymentRequestService != null;
        assert delegate != null;

        mPaymentRequestService = paymentRequestService;
        mRenderFrameHost = paymentRequestService.getRenderFrameHost();
        assert mRenderFrameHost != null;
        mDelegate = delegate;
        mWebContents = paymentRequestService.getWebContents();
        mJourneyLogger = paymentRequestService.getJourneyLogger();
        String topLevelOrigin = paymentRequestService.getTopLevelOrigin();
        assert topLevelOrigin != null;
        mPaymentUiService =
                mDelegate.createPaymentUiService(
                        /* delegate= */ this,
                        /* params= */ paymentRequestService,
                        mWebContents,
                        paymentRequestService.isOffTheRecord(),
                        mJourneyLogger,
                        topLevelOrigin);
        mDialogController =
                new DialogControllerImpl(
                        mWebContents,
                        (context, style) -> {
                            return new AlertDialog.Builder(context, style);
                        });
        mAndroidIntentLauncher = new WindowAndroidIntentLauncher(mWebContents);
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest()
                    .onPaymentUiServiceCreated(mPaymentUiService);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public PaymentApp getSelectedPaymentApp() {
        return mPaymentUiService.getSelectedPaymentApp();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public List<PaymentApp> getPaymentApps() {
        return mPaymentUiService.getPaymentApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAnyCompleteApp() {
        return mPaymentUiService.hasAnyCompleteAppSuggestion();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onSpecValidated(PaymentRequestSpec spec) {
        mSpec = spec;
        mPaymentUiService.initialize(mSpec.getPaymentDetails());
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean disconnectIfExtraValidationFails(
            WebContents webContents,
            Map<String, PaymentMethodData> methodData,
            PaymentDetails details,
            PaymentOptions options) {
        assert methodData != null;
        assert details != null;

        if (!parseAndValidateDetailsFurtherIfNeeded(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.INVALID_PAYMENT_DETAILS,
                    PaymentErrorReason.INVALID_DATA_FROM_RENDERER);
            return true;
        }
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String showOrSkipAppSelector(
            boolean isShowWaitingForUpdatedDetails,
            PaymentItem total,
            boolean shouldSkipAppSelector) {
        Activity activity = mDelegate.getActivity(mWebContents);
        if (activity == null) return ErrorStrings.ACTIVITY_NOT_FOUND;
        TabModelSelector tabModelSelector = mDelegate.getTabModelSelector(mWebContents);
        if (tabModelSelector == null) return ErrorStrings.TAB_NOT_FOUND;
        TabModel tabModel = mDelegate.getTabModel(mWebContents);
        if (tabModel == null) return ErrorStrings.TAB_NOT_FOUND;
        String error =
                mPaymentUiService.buildPaymentRequestUi(
                        /* isWebContentsActive= */ mDelegate.isWebContentsActive(mRenderFrameHost),
                        activity,
                        tabModelSelector,
                        tabModel);
        if (error != null) return error;
        // Calculate skip ui and build ui only after all payment apps are ready and
        // request.show() is called.
        boolean urlPaymentMethodIdentifiersSupported =
                PaymentRequestService.isUrlPaymentMethodIdentifiersSupported(
                        mSpec.getMethodData().keySet());
        // Only allowing payment apps that own their own UIs.
        if (!urlPaymentMethodIdentifiersSupported
                && !mSpec.isSecurePaymentConfirmationRequested()) {
            shouldSkipAppSelector = false;
        }

        if (shouldSkipAppSelector) {
            mHasSkippedAppSelector = true;
        } else {
            mPaymentUiService.showAppSelector(isShowWaitingForUpdatedDetails);
            mJourneyLogger.setShown();
        }
        return null;
    }

    private void dimBackgroundIfNotPaymentHandler(PaymentApp selectedApp) {
        if (selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP) {
            // As bottom-sheet itself has dimming effect, dimming PR is unnecessary for the
            // bottom-sheet PH. For now, service worker based payment apps are the only ones that
            // can open the bottom-sheet.
            return;
        }
        mPaymentUiService.dimBackground();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean showNoMatchingPaymentCredential() {
        assert mSpec != null;
        assert !mSpec.isDestroyed();
        assert mSpec.isSecurePaymentConfirmationRequested();
        assert !hasAvailableApps();

        mJourneyLogger.setNoMatchingCredentialsShown();
        mNoMatchingController =
                SecurePaymentConfirmationNoMatchingCredController.create(mWebContents);
        Runnable continueCallback =
                () -> {
                    mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
                    disconnectFromClientWithDebugMessage(
                            ErrorStrings.WEB_AUTHN_OPERATION_TIMED_OUT_OR_NOT_ALLOWED,
                            PaymentErrorReason.NOT_ALLOWED_ERROR);
                };
        Runnable optOutCallback =
                () -> {
                    mJourneyLogger.setAborted(AbortReason.USER_OPTED_OUT);
                    disconnectFromClientWithDebugMessage(
                            ErrorStrings.SPC_USER_OPTED_OUT, PaymentErrorReason.USER_OPT_OUT);
                };
        PaymentMethodData spcMethodData =
                mSpec.getMethodData().get(MethodStrings.SECURE_PAYMENT_CONFIRMATION);
        assert spcMethodData != null;
        mNoMatchingController.show(
                continueCallback,
                optOutCallback,
                spcMethodData.securePaymentConfirmation.showOptOut,
                spcMethodData.securePaymentConfirmation.rpId);

        return true;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        assert mSpec.getRawTotal() != null;

        if (isSecurePaymentConfirmationApplicable()) {
            assert getSelectedPaymentApp() != null;
            assert mSpcAuthnUiController == null;

            mSpcAuthnUiController = SecurePaymentConfirmationAuthnController.create(mWebContents);
            PaymentMethodData spcMethodData =
                    mSpec.getMethodData().get(MethodStrings.SECURE_PAYMENT_CONFIRMATION);
            assert spcMethodData != null;
            Origin payeeOrigin =
                    spcMethodData.securePaymentConfirmation.payeeOrigin != null
                            ? new Origin(spcMethodData.securePaymentConfirmation.payeeOrigin)
                            : null;
            Callback<Boolean> responseCallback =
                    (response) -> {
                        if (response) {
                            onSecurePaymentConfirmationUiAccepted(getSelectedPaymentApp());
                        } else {
                            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
                            disconnectFromClientWithDebugMessage(
                                    ErrorStrings.WEB_AUTHN_OPERATION_TIMED_OUT_OR_NOT_ALLOWED,
                                    PaymentErrorReason.NOT_ALLOWED_ERROR);
                        }
                        mSpcAuthnUiController = null;
                    };
            Runnable optOutCallback =
                    () -> {
                        mJourneyLogger.setAborted(AbortReason.USER_OPTED_OUT);
                        disconnectFromClientWithDebugMessage(
                                ErrorStrings.SPC_USER_OPTED_OUT, PaymentErrorReason.USER_OPT_OUT);
                        mSpcAuthnUiController = null;
                    };
            boolean success =
                    mSpcAuthnUiController.show(
                            getSelectedPaymentApp().getDrawableIcon(),
                            getSelectedPaymentApp().getLabel(),
                            mSpec.getRawTotal(),
                            responseCallback,
                            optOutCallback,
                            spcMethodData.securePaymentConfirmation.payeeName,
                            payeeOrigin,
                            spcMethodData.securePaymentConfirmation.showOptOut,
                            spcMethodData.securePaymentConfirmation.rpId);

            if (success) {
                mJourneyLogger.setShown();
                mPaymentRequestService.onUiDisplayed();
                return null;
            } else {
                mSpcAuthnUiController = null;
                return ErrorStrings.SPC_AUTHN_UI_SUPPRESSED;
            }
        }

        WindowAndroid windowAndroid = mDelegate.getWindowAndroid(mRenderFrameHost);
        if (windowAndroid == null) return ErrorStrings.WINDOW_NOT_FOUND;
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        // If we are skipping showing the app selector UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if (mHasSkippedAppSelector) {
            assert !mPaymentUiService.getPaymentApps().isEmpty();
            PaymentApp selectedApp = getSelectedPaymentApp();
            dimBackgroundIfNotPaymentHandler(selectedApp);
            mJourneyLogger.setSkippedShow();
            invokePaymentApp(
                    /* selectedShippingAddress= */ null,
                    /* selectedShippingOption= */ null,
                    selectedApp);
        } else {
            mPaymentUiService.createShippingSectionIfNeeded(context);
        }

        return null;
    }

    private boolean isSecurePaymentConfirmationApplicable() {
        PaymentApp selectedApp = mPaymentUiService.getSelectedPaymentApp();
        // TODO(crbug.com/40767878): Deduplicate this part with
        // SecurePaymentConfirmationController::SetupModelAndShowDialogIfApplicable().
        return selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.INTERNAL
                && selectedApp.getInstrumentMethodNames().size() == 1
                && selectedApp
                        .getInstrumentMethodNames()
                        .contains(MethodStrings.SECURE_PAYMENT_CONFIRMATION)
                && getPaymentApps().size() == 1
                && mSpec != null
                && !mSpec.isDestroyed()
                && mSpec.isSecurePaymentConfirmationRequested()
                && !PaymentOptionsUtils.requestAnyInformation(mSpec.getPaymentOptions());
    }

    private void onSecurePaymentConfirmationUiAccepted(PaymentApp app) {
        assert mPaymentRequestService != null;
        mPaymentRequestService.invokePaymentApp(
                app, new PaymentResponseHelper(app, mSpec.getPaymentOptions()));
    }

    // Implements BrowserPaymentRequest:
    @Override
    public @Nullable WebContents openPaymentHandlerWindow(GURL url, long ukmSourceId) {
        @Nullable
        WebContents paymentHandlerWebContents = mPaymentUiService.showPaymentHandlerUi(url);
        if (paymentHandlerWebContents != null) {
            ServiceWorkerPaymentAppBridge.onOpeningPaymentAppWindow(
                    /* paymentRequestWebContents= */ mWebContents,
                    /* paymentHandlerWebContents= */ paymentHandlerWebContents);

            // UKM for payment app origin should get recorded only when the origin of the invoked
            // payment app is shown to the user.
            mJourneyLogger.setPaymentAppUkmSourceId(ukmSourceId);
        }
        return paymentHandlerWebContents;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
        mPaymentUiService.updateDetailsOnPaymentRequestUi(details);

        if (hasNotifiedInvokedPaymentApp) return;

        mPaymentUiService.showShippingAddressErrorIfApplicable(details.error);
        mPaymentUiService.enableAndUpdatePaymentRequestUiWithPaymentInfo();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String continueShowWithUpdatedDetails(
            PaymentDetails details, boolean isFinishedQueryingPaymentApps) {
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        mPaymentUiService.updateDetailsOnPaymentRequestUi(details);

        if (isFinishedQueryingPaymentApps && !mHasSkippedAppSelector) {
            mPaymentUiService.enableAndUpdatePaymentRequestUiWithPaymentInfo();
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        mPaymentUiService.showShippingAddressErrorIfApplicable(selectedShippingOptionError);
        mPaymentUiService.enableAndUpdatePaymentRequestUiWithPaymentInfo();
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean invokePaymentApp(
            EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption,
            PaymentApp selectedPaymentApp) {
        if (mPaymentRequestService == null || mSpec == null || mSpec.isDestroyed()) return false;
        selectedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        PaymentResponseHelperInterface paymentResponseHelper =
                new ChromePaymentResponseHelper(
                        selectedShippingAddress,
                        selectedShippingOption,
                        mPaymentUiService.getSelectedContact(),
                        selectedPaymentApp,
                        mSpec.getPaymentOptions(),
                        PersonalDataManagerFactory.getForProfile(
                                Profile.fromWebContents(mWebContents)));
        mPaymentRequestService.invokePaymentApp(selectedPaymentApp, paymentResponseHelper);
        return true;
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost =
                    mDelegate.createPaymentHandlerHost(
                            mWebContents, /* listener= */ mPaymentRequestService);
        }
        return mPaymentHandlerHost;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean wasRetryCalled() {
        return mWasRetryCalled;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void onUiAborted(@AbortReason int reason, String debugMessage) {
        mJourneyLogger.setAborted(reason);
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    /**
     * Sends the debugMessage and paymentErrorReason to the renderer and closes the mojo IPC
     * connection to it.
     *
     * @param debugMessage Web-developer facing error message.
     * @param paymentErrorReason A value from PaymentErrorReason enum that determines the HTML error
     *     code returned in JavaScript API.
     */
    private void disconnectFromClientWithDebugMessage(String debugMessage, int paymentErrorReason) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(
                    debugMessage, paymentErrorReason);
        }
        close();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
        if (result != PaymentComplete.FAIL && !PaymentPreferencesUtil.isPaymentCompleteOnce()) {
            PaymentPreferencesUtil.setPaymentCompleteOnce();
        }

        mPaymentUiService.onPaymentRequestComplete(result, onCompleteHandled);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {
        mWasRetryCalled = true;
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) {
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.CONTEXT_NOT_FOUND, PaymentErrorReason.UNKNOWN);
            return;
        }
        mPaymentUiService.onRetry(context, errors);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mSpcAuthnUiController != null) {
            mSpcAuthnUiController.hide();
            mSpcAuthnUiController = null;
        }

        if (mNoMatchingController != null) {
            mNoMatchingController.close();
            mNoMatchingController = null;
        }

        if (mPaymentRequestService != null) {
            mPaymentRequestService.close();
            mPaymentRequestService = null;
        }

        mPaymentUiService.close();

        if (mPaymentHandlerHost != null) {
            mPaymentHandlerHost.destroy();
            mPaymentHandlerHost = null;
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {
        mPaymentUiService.setPaymentApps(pendingApps);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        return mPaymentUiService.hasAvailableApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsReady() {
        // Showing the app selector UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mHasSkippedAppSelector) {
            mPaymentUiService.showProcessingMessageAfterUiSkip();
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasSkippedAppSelector() {
        return mHasSkippedAppSelector;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void showAppSelectorAfterPaymentAppInvokeFailed() {
        mPaymentUiService.onPayButtonProcessingCancelled();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isShippingSectionVisible() {
        return mPaymentUiService.shouldShowShippingSection();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isContactSectionVisible() {
        return mPaymentUiService.shouldShowContactSection();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public DialogController getDialogController() {
        return mDialogController;
    }

    // Implements BrowserPaymentRequest:
    @Override
    @Nullable
    public byte[][] getCertificateChain() {
        if (mCertificateChain == null
                && !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.ANDROID_PAYMENT_INTENTS_OMIT_DEPRECATED_PARAMETERS)) {
            mCertificateChain = CertificateChainHelper.getCertificateChain(mWebContents);
        }

        return mCertificateChain;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public AndroidIntentLauncher getAndroidIntentLauncher() {
        return mAndroidIntentLauncher;
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail) {
        if (mPaymentRequestService == null || !mWasRetryCalled) return;
        mPaymentRequestService.onPayerDetailChange(detail);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onPaymentRequestUiFaviconNotAvailable() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.warnNoFavicon();
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onShippingOptionChange(String optionId) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onShippingOptionChange(optionId);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onLeavingCurrentTab(String reason) {
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(reason, PaymentErrorReason.USER_CANCEL);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onUiServiceError(String error) {
        mJourneyLogger.setAborted(AbortReason.OTHER);
        disconnectFromClientWithDebugMessage(error, PaymentErrorReason.USER_CANCEL);
        if (PaymentRequestService.getObserverForTest() != null) {
            PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
        }
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onShippingAddressChange(PaymentAddress address) {
        if (mPaymentRequestService == null) return;
        // This updates the line items and the shipping options asynchronously.
        mPaymentRequestService.onShippingAddressChange(address);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public @Nullable Context getContext() {
        return mDelegate.getContext(mRenderFrameHost);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public @Nullable ActivityLifecycleDispatcher getActivityLifecycleDispatcher() {
        return mDelegate.getActivityLifecycleDispatcher(mWebContents);
    }

    @VisibleForTesting
    @Nullable
    public SecurePaymentConfirmationAuthnController
            getSecurePaymentConfirmationAuthnUiForTesting() {
        return mSpcAuthnUiController;
    }

    @VisibleForTesting
    @Nullable
    public SecurePaymentConfirmationNoMatchingCredController
            getSecurePaymentConfirmationNoMatchingCredUiForTesting() {
        return mNoMatchingController;
    }
}
