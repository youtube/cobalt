// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.autofill.AutofillRequest.FocusField;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This class works with Android autofill service to fill web form, it doesn't use chrome's
 * autofill service or suggestion UI. All methods are supposed to be called in UI thread.
 *
 * AutofillProvider handles one autofill session at time, each call of
 * queryFormFieldAutofill cancels previous session and starts a new one, the
 * calling of other methods shall associate with current session.
 *
 * This class doesn't have 1:1 mapping to native AutofillProviderAndroid; the
 * normal ownership model is that this object is owned by the embedder-specific
 * Java WebContents wrapper (e.g., AwContents.java in //android_webview), and
 * AutofillProviderAndroid is owned by the embedder-specific C++ WebContents
 * wrapper (e.g., native AwContents in //android_webview).
 *
 */
@RequiresApi(Build.VERSION_CODES.O)
@JNINamespace("autofill")
public class AutofillProvider {
    /**
     * Factory interface for testing. AutofillManagerWrapper must be created in AutofillProvider
     * constructor.
     */
    public static interface AutofillManagerWrapperFactoryForTesting {
        AutofillManagerWrapper create(Context context);
    }

    private static AutofillManagerWrapperFactoryForTesting sAutofillManagerForTestingFactory;

    private final String mProviderName;
    private AutofillManagerWrapper mAutofillManager;
    private ViewGroup mContainerView;
    private WebContents mWebContents;

    private AutofillRequest mRequest;
    private long mNativeAutofillProvider;
    private AutofillProviderUMA mAutofillUMA;
    private AutofillManagerWrapper.InputUIObserver mInputUIObserver;
    private long mAutofillTriggeredTimeMillis;
    private Context mContext;
    private AutofillPopup mDatalistPopup;
    private AutofillSuggestion[] mDatalistSuggestions;
    private WebContentsAccessibility mWebContentsAccessibility;
    private View mAnchorView;

    public AutofillProvider(Context context, ViewGroup containerView, WebContents webContents,
            String providerName) {
        mWebContents = webContents;
        mProviderName = providerName;
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("AutofillProvider.constructor")) {
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
            if (sAutofillManagerForTestingFactory != null) {
                mAutofillManager = sAutofillManagerForTestingFactory.create(context);
            } else {
                mAutofillManager = new AutofillManagerWrapper(context);
            }
            mContainerView = containerView;
            mAutofillUMA = new AutofillProviderUMA(
                    context, mAutofillManager.isAwGCurrentAutofillService());
            mInputUIObserver = new AutofillManagerWrapper.InputUIObserver() {
                @Override
                public void onInputUIShown() {
                    // Not need to report suggestion window displayed if there is no live autofill
                    // session.
                    if (mRequest == null) return;
                    mAutofillUMA.onSuggestionDisplayed(
                            System.currentTimeMillis() - mAutofillTriggeredTimeMillis);
                }
            };
            mAutofillManager.addInputUIObserver(mInputUIObserver);
            mContext = context;
        }
        mNativeAutofillProvider = initializeNativeAutofillProvider(webContents);
    }

    public void destroy() {
        detachFromJavaAutofillProvider();
        mAutofillManager.destroy();
    }

    /**
     * Invoked when container view is changed.
     *
     * @param containerView new container view.
     */
    public void onContainerViewChanged(ViewGroup containerView) {
        mContainerView = containerView;
    }

    /**
     * Invoked when autofill service needs the form structure.
     *
     * @param structure see View.onProvideAutofillVirtualStructure()
     * @param flags see View.onProvideAutofillVirtualStructure()
     */
    public void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags) {
        // This method could be called for the session started by the native
        // control outside of the scope of autofill, e.g. the URL bar, in this case, we simply
        // return.
        if (mRequest == null) return;

        Bundle bundle = structure.getExtras();
        if (bundle != null) {
            bundle.putCharSequence("VIRTUAL_STRUCTURE_PROVIDER_NAME", mProviderName);
            bundle.putCharSequence(
                    "VIRTUAL_STRUCTURE_PROVIDER_VERSION", VersionConstants.PRODUCT_VERSION);
            AutofillHintsService autofillHintsService = mRequest.getAutofillHintsService();
            if (autofillHintsService != null) {
                bundle.putBinder("AUTOFILL_HINTS_SERVICE", autofillHintsService.getBinder());
            }
        }
        mRequest.fillViewStructure(structure);
        if (AutofillManagerWrapper.isLoggable()) {
            AutofillManagerWrapper.log(
                    "onProvideAutoFillVirtualStructure fields:" + structure.getChildCount());
        }
        mAutofillUMA.onVirtualStructureProvided();
    }

    /**
     * Invoked when autofill value is available, AutofillProvider shall fill the
     * form with the provided values.
     *
     * @param values the array of autofill values, the key is virtual id of form
     *            field.
     */
    public void autofill(final SparseArray<AutofillValue> values) {
        if (mNativeAutofillProvider != 0 && mRequest != null && mRequest.autofill((values))) {
            autofill(mNativeAutofillProvider, mRequest.getForm());
            if (AutofillManagerWrapper.isLoggable()) {
                AutofillManagerWrapper.log("autofill values:" + values.size());
            }
            mAutofillUMA.onAutofill();
        }
    }

    /**
     * @return whether query autofill suggestion.
     */
    public boolean shouldQueryAutofillSuggestion() {
        return mRequest != null && mRequest.getFocusField() != null
                && !mAutofillManager.isAutofillInputUIShowing();
    }

    public void queryAutofillSuggestion() {
        if (shouldQueryAutofillSuggestion()) {
            FocusField focusField = mRequest.getFocusField();
            mAutofillManager.requestAutofill(mContainerView,
                    mRequest.getVirtualId(focusField.fieldIndex), focusField.absBound);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static void setAutofillManagerWrapperFactoryForTesting(
            AutofillManagerWrapperFactoryForTesting factory) {
        sAutofillManagerForTestingFactory = factory;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void replaceAutofillManagerWrapperForTesting(AutofillManagerWrapper wrapper) {
        mAutofillManager = wrapper;
    }

    /**
     * Invoked when filling form is need. AutofillProvider shall ask autofill
     * service for the values with which to fill the form.
     *
     * @param formData the form needs to fill.
     * @param focus the index of focus field in formData
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     * @param hasServerPrediction whether the server prediction arrived.
     */
    @CalledByNative
    public void startAutofillSession(FormData formData, int focus, float x, float y, float width,
            float height, boolean hasServerPrediction) {
        // Check focusField inside short value?
        // Autofill Manager might have session that wasn't started by AutofillProvider,
        // we just always cancel existing session here.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            mAutofillManager.cancel();
        }

        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        if (mRequest != null) notifyViewExitBeforeDestroyRequest();
        transformFormFieldToContainViewCoordinates(formData);
        mRequest = new AutofillRequest(
                formData, new FocusField((short) focus, absBound), hasServerPrediction);
        int virtualId = mRequest.getVirtualId((short) focus);
        notifyVirtualViewEntered(mContainerView, virtualId, absBound);
        mAutofillUMA.onSessionStarted(mAutofillManager.isDisabled());
        if (hasServerPrediction) {
            mAutofillUMA.onServerTypeAvailable(formData, /*afterSessionStarted=*/false);
        }
        mAutofillTriggeredTimeMillis = System.currentTimeMillis();

        mAutofillManager.notifyNewSessionStarted(hasServerPrediction);
    }

    /**
     * Invoked when form field's value is changed.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    public void onFormFieldDidChange(int index, float x, float y, float width, float height) {
        // Check index inside short value?
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) {
            onFocusChangedImpl(true, index, x, y, width, height, true /*causedByValueChange*/);
        } else {
            // Currently there is no api to notify both value and position
            // change, before the API is available, we still need to call
            // notifyVirtualViewEntered() to tell current coordinates because
            // the position could be changed.
            int virtualId = mRequest.getVirtualId(sIndex);
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (!focusField.absBound.equals(absBound)) {
                notifyVirtualViewExited(mContainerView, virtualId);
                notifyVirtualViewEntered(mContainerView, virtualId, absBound);
                // Update focus field position.
                mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
            }
        }
        notifyVirtualValueChanged(index, /* forceNotify = */ false);
        mAutofillUMA.onUserChangeFieldValue(mRequest.getField(sIndex).hasPreviouslyAutofilled());
    }

    /**
     * Invoked when text field is scrolled.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    public void onTextFieldDidScroll(int index, float x, float y, float width, float height) {
        if (mRequest == null) return;

        short sIndex = (short) index;
        FormFieldData fieldData = mRequest.getField(sIndex);
        if (fieldData != null) fieldData.updateBounds(new RectF(x, y, x + width, y + height));

        // crbug.com/730764 - from P and above, Android framework listens to the onScrollChanged()
        // and repositions the autofill UI automatically.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) return;

        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) return;

        int virtualId = mRequest.getVirtualId(sIndex);
        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        // Notify the new position to the Android framework. Note that we do not call
        // notifyVirtualViewExited() here intentionally to avoid flickering.
        notifyVirtualViewEntered(mContainerView, virtualId, absBound);

        // Update focus field position.
        mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
    }

    // Add a specific method in order to mock it in test.
    protected long initializeNativeAutofillProvider(WebContents webContents) {
        return AutofillProviderJni.get().init(this, webContents);
    }

    private boolean isDatalistField(int childId) {
        FormFieldData field = mRequest.getField((short) childId);
        return field.mControlType == FormFieldData.ControlType.DATALIST;
    }

    private void notifyVirtualValueChanged(int index, boolean forceNotify) {
        // The ValueChanged, ViewEntered and ViewExited aren't notified to the autofill service for
        // the focused datalist to avoid the potential UI conflict.
        // The datalist support was added later and the option list is displayed by WebView, the
        // autofill service might also show its suggestions when the datalist (associated the input
        // field) is focused, the two UI overlap, the solution is to completely hide the fact that
        // the datalist is being focused to the autofill service to prevent it from displaying the
        // suggestion.
        // The ValueChange will still be sent to autofill service when the form
        // submitted or autofilled.
        if (!forceNotify && isDatalistField(index)) return;
        AutofillValue autofillValue = mRequest.getFieldNewValue(index);
        if (autofillValue == null) return;
        mAutofillManager.notifyVirtualValueChanged(
                mContainerView, mRequest.getVirtualId((short) index), autofillValue);
    }

    private void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(childId)) return;
        mAutofillManager.notifyVirtualViewEntered(parent, childId, absBounds);
    }

    private void notifyVirtualViewExited(View parent, int childId) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(childId)) return;
        mAutofillManager.notifyVirtualViewExited(parent, childId);
    }

    /**
     * Invoked when current form will be submitted.
     * @param submissionSource the submission source, could be any member defined in
     * SubmissionSource.java
     */
    @CalledByNative
    public void onFormSubmitted(int submissionSource) {
        // The changes could be missing, like those made by Javascript, we'd better to notify
        // AutofillManager current values. also see crbug.com/353001 and crbug.com/732856.
        forceNotifyFormValues();
        mAutofillManager.commit(submissionSource);
        mRequest = null;
        mAutofillUMA.onFormSubmitted(submissionSource);
    }

    /**
     * Invoked when focus field changed.
     *
     * @param focusOnForm whether focus is still on form.
     * @param focusItem the index of field has focus
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     */
    @CalledByNative
    public void onFocusChanged(
            boolean focusOnForm, int focusField, float x, float y, float width, float height) {
        onFocusChangedImpl(
                focusOnForm, focusField, x, y, width, height, false /*causedByValueChange*/);
    }

    @CalledByNative
    public void hidePopup() {
        if (mDatalistPopup != null) {
            mDatalistPopup.dismiss();
            mDatalistPopup = null;
            mDatalistSuggestions = null;
        }
        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.onAutofillPopupDismissed();
        }
    }

    private void notifyViewExitBeforeDestroyRequest() {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null) return;
        notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(focusField.fieldIndex));
        mRequest.setFocusField(null);
    }

    private void onFocusChangedImpl(boolean focusOnForm, int focusField, float x, float y,
            float width, float height, boolean causedByValueChange) {
        // Check focusField inside short value?
        // FocusNoLongerOnForm is called after form submitted.
        if (mRequest == null) return;
        FocusField prev = mRequest.getFocusField();
        if (focusOnForm) {
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (prev != null && prev.fieldIndex == focusField && absBound.equals(prev.absBound)) {
                return;
            }

            // Notify focus changed.
            if (prev != null) {
                notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            }

            notifyVirtualViewEntered(
                    mContainerView, mRequest.getVirtualId((short) focusField), absBound);

            if (!causedByValueChange) {
                // The focus field value might not sync with platform's
                // AutofillManager, just notify it value changed.
                notifyVirtualValueChanged(focusField, /* forceNotify = */ false);
                mAutofillTriggeredTimeMillis = System.currentTimeMillis();
            }
            mRequest.setFocusField(new FocusField((short) focusField, absBound));
        } else {
            if (prev == null) return;
            // Notify focus changed.
            notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            mRequest.setFocusField(null);
        }
    }

    @CalledByNative
    protected void showDatalistPopup(
            String[] datalistValues, String[] datalistLabels, boolean isRtl) {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField != null) {
            showDatalistPopup(datalistValues, datalistLabels,
                    mRequest.getField(focusField.fieldIndex).getBounds(), isRtl);
        }
    }

    /**
     * Display the simplest popup for the datalist. This is same as WebView's datalist popup in
     * Android pre-o. No suggestion from the autofill service will be presented, No advance
     * features of AutofillPopup are used.
     */
    private void showDatalistPopup(
            String[] datalistValues, String[] datalistLabels, RectF bounds, boolean isRtl) {
        mDatalistSuggestions = new AutofillSuggestion[datalistValues.length];
        for (int i = 0; i < mDatalistSuggestions.length; i++) {
            mDatalistSuggestions[i] = new AutofillSuggestion(datalistValues[i], datalistLabels[i],
                    /* itemTag= */ "", DropdownItem.NO_ICON, false /* isIconAtLeft */, i,
                    false /* isDeletable */, false /* isMultilineLabel */, false /* isBoldLabel */,
                    /* featureForIPH= */ "");
        }
        if (mWebContentsAccessibility == null) {
            mWebContentsAccessibility = WebContentsAccessibility.fromWebContents(mWebContents);
        }
        if (mDatalistPopup == null) {
            if (ContextUtils.activityFromContext(mContext) == null) return;
            ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
            if (mAnchorView == null) mAnchorView = delegate.acquireView();
            setAnchorViewRect(bounds);
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mDatalistPopup = new AutofillPopup(mContext, mAnchorView, new AutofillDelegate() {
                    @Override
                    public void dismissed() {
                        onDatalistPopupDismissed();
                    }

                    @Override
                    public void suggestionSelected(int listIndex) {
                        onSuggestionSelected(mDatalistSuggestions[listIndex].getLabel());
                    }

                    @Override
                    public void deleteSuggestion(int listIndex) {}

                    @Override
                    public void accessibilityFocusCleared() {
                        mWebContentsAccessibility.onAutofillPopupAccessibilityFocusCleared();
                    }
                }, null);
            } catch (RuntimeException e) {
                // Deliberately swallowing exception because bad framework implementation can
                // throw exceptions in ListPopupWindow constructor.
                onDatalistPopupDismissed();
                return;
            }
        }
        mDatalistPopup.filterAndShow(mDatalistSuggestions, isRtl);
        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.onAutofillPopupDisplayed(mDatalistPopup.getListView());
        }
    }

    private void onDatalistPopupDismissed() {
        ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
        delegate.removeView(mAnchorView);
        mAnchorView = null;
    }

    private void onSuggestionSelected(String value) {
        if (mNativeAutofillProvider != 0) {
            acceptDataListSuggestion(mNativeAutofillProvider, value);
        }
        hidePopup();
    }

    private void setAnchorViewRect(RectF rect) {
        if (mNativeAutofillProvider != 0) {
            setAnchorViewRect(mNativeAutofillProvider, mAnchorView, rect);
        }
    }

    /**
     * Invoked when current query need to be reset.
     */
    @CalledByNative
    protected void reset() {
        // We don't need to reset anything here, it should be safe to cancel
        // current autofill session when new one starts in
        // startAutofillSession().
    }

    @CalledByNative
    protected void setNativeAutofillProvider(long nativeAutofillProvider) {
        if (nativeAutofillProvider == mNativeAutofillProvider) return;
        // Setting the mNativeAutofillProvider to 0 may occur as a
        // result of WebView.destroy, or because a WebView has been
        // gc'ed. In the former case we can go ahead and clean up the
        // frameworks autofill manager, but in the latter case the
        // binder connection has already been dropped in a framework
        // finalizer, and so the methods we call will throw. It's not
        // possible to know which case we're in, so just catch the exception
        // in AutofillManagerWrapper.destroy().
        if (mNativeAutofillProvider != 0) mRequest = null;
        mNativeAutofillProvider = nativeAutofillProvider;
    }

    private void detachFromJavaAutofillProvider() {
        if (mNativeAutofillProvider == 0) return;
        // Makes sure this is the last call to mNativeAutofillProvider.
        long nativeAutofillProvider = mNativeAutofillProvider;
        mNativeAutofillProvider = 0;
        AutofillProviderJni.get().detachFromJavaAutofillProvider(nativeAutofillProvider);
    }

    public void setWebContents(WebContents webContents) {
        if (webContents == mWebContents) return;
        if (mWebContents != null) mRequest = null;
        mWebContents = webContents;
        detachFromJavaAutofillProvider();
        if (mWebContents != null) {
            initializeNativeAutofillProvider(webContents);
        }
    }

    @CalledByNative
    protected void onDidFillAutofillFormData() {
        // The changes were caused by the autofill service autofill form,
        // notified it about the result.
        forceNotifyFormValues();
    }

    @CalledByNative
    private void onQueryDone(boolean success) {
        if (mRequest == null) return;
        mRequest.onQueryDone(success);
        mAutofillUMA.onServerTypeAvailable(
                success ? mRequest.getForm() : null, /*afterSessionStarted*/ true);
        mAutofillManager.onQueryDone(success);
    }

    private void forceNotifyFormValues() {
        if (mRequest == null) return;
        for (int i = 0; i < mRequest.getFieldCount(); ++i) {
            notifyVirtualValueChanged(i, /* forceNotify = */ true);
        }
    }

    @VisibleForTesting
    public AutofillPopup getDatalistPopupForTesting() {
        return mDatalistPopup;
    }

    private Rect transformToWindowBounds(RectF rect) {
        // Refer to crbug.com/1085294 for the reason of offset.
        // The current version of Mockito didn't support mock static method, adding extra method so
        // the transform can be tested.
        return transformToWindowBoundsWithOffsetY(
                rect, RenderCoordinates.fromWebContents(mWebContents).getContentOffsetYPixInt());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public Rect transformToWindowBoundsWithOffsetY(RectF rect, int offsetY) {
        // Convert bounds to device pixel.
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        RectF bounds = new RectF(rect);
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        int[] location = new int[2];
        mContainerView.getLocationOnScreen(location);
        location[1] += offsetY;
        matrix.postTranslate(location[0], location[1]);
        matrix.mapRect(bounds);
        return new Rect(
                (int) bounds.left, (int) bounds.top, (int) bounds.right, (int) bounds.bottom);
    }

    /**
     * Transform FormFieldData's bounds to ContainView's coordinates and update the bounds with the
     * transformed one.
     *
     * @param formData the form need to be transformed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void transformFormFieldToContainViewCoordinates(FormData formData) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        matrix.postTranslate(mContainerView.getScrollX(), mContainerView.getScrollY());

        for (FormFieldData field : formData.mFields) {
            RectF bounds = new RectF();
            matrix.mapRect(bounds, field.getBounds());
            field.setBoundsInContainerViewCoordinates(bounds);
        }
    }

    /**
     * Send form to renderer for filling.
     *
     * @param nativeAutofillProvider the native autofill provider.
     * @param formData the form to fill.
     */
    private void autofill(long nativeAutofillProvider, FormData formData) {
        AutofillProviderJni.get().onAutofillAvailable(
                nativeAutofillProvider, AutofillProvider.this, formData);
    }

    private void acceptDataListSuggestion(long nativeAutofillProvider, String value) {
        AutofillProviderJni.get().onAcceptDataListSuggestion(
                nativeAutofillProvider, AutofillProvider.this, value);
    }

    private void setAnchorViewRect(long nativeAutofillProvider, View anchorView, RectF rect) {
        AutofillProviderJni.get().setAnchorViewRect(nativeAutofillProvider, AutofillProvider.this,
                anchorView, rect.left, rect.top, rect.width(), rect.height());
    }

    @NativeMethods
    interface Natives {
        long init(AutofillProvider caller, WebContents webContents);
        void detachFromJavaAutofillProvider(long nativeAutofillProviderAndroid);
        void onAutofillAvailable(
                long nativeAutofillProviderAndroid, AutofillProvider caller, FormData formData);
        void onAcceptDataListSuggestion(
                long nativeAutofillProviderAndroid, AutofillProvider caller, String value);
        void setAnchorViewRect(long nativeAutofillProviderAndroid, AutofillProvider caller,
                View anchorView, float x, float y, float width, float height);
    }
}
