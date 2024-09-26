// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressField;
import org.chromium.components.autofill.EditableOption;
import org.chromium.payments.mojom.PaymentAddress;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * The locally stored autofill address.
 */
public class AutofillAddress extends EditableOption {
    /** The pattern for a valid region code. */
    private static final String REGION_CODE_PATTERN = "^[A-Z]{2}$";

    // Bit field values are identical to ProfileFields in payments_profile_comparator.h.
    @IntDef({CompletionStatus.COMPLETE, CompletionStatus.INVALID_RECIPIENT,
            CompletionStatus.INVALID_PHONE_NUMBER, CompletionStatus.INVALID_ADDRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CompletionStatus {
        /** Can be sent to the merchant as-is without editing first. */
        int COMPLETE = 0;
        /** The recipient is missing. */
        int INVALID_RECIPIENT = 1 << 0;
        /** The phone number is invalid or missing. */
        int INVALID_PHONE_NUMBER = 1 << 1;
        /**
         * The address is invalid. For example, missing state or city name. To have consistent bit
         * field values between Android and Desktop 1 << 2 is reserved for email address.
         */
        int INVALID_ADDRESS = 1 << 3;
        int MAX_VALUE = 1 << 4;
    }

    @Nullable private static Pattern sRegionCodePattern;

    private final Context mContext;
    private AutofillProfile mProfile;
    @Nullable private String mShippingLabelWithCountry;
    @Nullable private String mShippingLabelWithoutCountry;
    @Nullable private String mBillingLabel;

    /**
     * Builds the autofill address.
     *
     * @param context The context where this address was created.
     * @param profile The autofill profile containing the address information.
     */
    public AutofillAddress(Context context, AutofillProfile profile) {
        super(profile.getGUID(), profile.getFullName(), profile.getLabel(),
                profile.getPhoneNumber(), null);
        mContext = context;
        mProfile = profile;
        mIsEditable = true;
        checkAndUpdateAddressCompleteness();
    }

    /** @return The autofill profile where this address data lives. */
    public AutofillProfile getProfile() {
        return mProfile;
    }

    /**
     * Updates the address and marks it "complete".
     * Called after the user has edited this address.
     * Updates the identifier and labels.
     *
     * @param profile The new profile to use.
     */
    public void completeAddress(AutofillProfile profile) {
        updateAddress(profile);
        assert mIsComplete;
    }

    /**
     * Updates the address and its completeness if needed.
     * Called after the user has edited this address.
     * Updates the identifier and labels.
     *
     * @param profile The new profile to use.
     */
    public void updateAddress(AutofillProfile profile) {
        // Since the profile changed, our cached labels are now out of date. Set them to null so the
        // labels are recomputed next time they are needed.
        mShippingLabelWithCountry = null;
        mShippingLabelWithoutCountry = null;
        mBillingLabel = null;

        mProfile = profile;
        updateIdentifierAndLabels(mProfile.getGUID(), mProfile.getFullName(), mProfile.getLabel(),
                mProfile.getPhoneNumber());
        checkAndUpdateAddressCompleteness();
    }

    /**
     * Gets the shipping address label which includes the country for the profile associated with
     * this address and sets it as sublabel for this EditableOption.
     */
    public void setShippingAddressLabelWithCountry() {
        assert mProfile != null;

        if (mShippingLabelWithCountry == null) {
            mShippingLabelWithCountry =
                    PersonalDataManager.getInstance()
                            .getShippingAddressLabelWithCountryForPaymentRequest(mProfile);
        }

        mProfile.setLabel(mShippingLabelWithCountry);
        updateSublabel(mProfile.getLabel());
    }

    /**
     * Gets the shipping address label which does not include the country for the profile associated
     * with this address and sets it as sublabel for this EditableOption.
     */
    public void setShippingAddressLabelWithoutCountry() {
        assert mProfile != null;

        if (mShippingLabelWithoutCountry == null) {
            mShippingLabelWithoutCountry =
                    PersonalDataManager.getInstance()
                            .getShippingAddressLabelWithoutCountryForPaymentRequest(mProfile);
        }

        mProfile.setLabel(mShippingLabelWithoutCountry);
        updateSublabel(mProfile.getLabel());
    }

    /*
     * Gets the billing address label for the profile associated with this address and sets it as
     * sublabel for this EditableOption.
     */
    public void setBillingAddressLabel() {
        assert mProfile != null;

        if (mBillingLabel == null) {
            mBillingLabel =
                    PersonalDataManager.getInstance().getBillingAddressLabelForPaymentRequest(
                            mProfile);
        }

        mProfile.setLabel(mBillingLabel);
        updateSublabel(mProfile.getLabel());
    }

    /**
     * Checks whether this address is complete and updates edit message, edit title and complete
     * status.
     */
    private void checkAndUpdateAddressCompleteness() {
        Pair<Integer, Integer> messageResIds =
                getEditMessageAndTitleResIds(checkAddressCompletionStatus(mProfile));

        mEditMessage = messageResIds.first.intValue() == 0
                ? null
                : mContext.getString(messageResIds.first);
        mEditTitle = messageResIds.second.intValue() == 0
                ? null
                : mContext.getString(messageResIds.second);
        mIsComplete = mEditMessage == null;
        mCompletenessScore = calculateCompletenessScore();
    }

    /**
     * Gets the edit message and title resource Ids for the completion status.
     *
     * @param  completionStatus The completion status.
     * @return The resource Ids. The first is the edit message resource Id. The second is the
     *         correspond editor title resource Id.
     */
    public static Pair<Integer, Integer> getEditMessageAndTitleResIds(
            @CompletionStatus int completionStatus) {
        int editMessageResId = 0;
        int editTitleResId = 0;

        switch (completionStatus) {
            case CompletionStatus.COMPLETE:
                editTitleResId = R.string.payments_edit_address;
                break;
            case CompletionStatus.INVALID_RECIPIENT:
                editMessageResId = R.string.payments_recipient_required;
                editTitleResId = R.string.payments_add_recipient;
                break;
            case CompletionStatus.INVALID_PHONE_NUMBER:
                editMessageResId = R.string.payments_phone_number_required;
                editTitleResId = R.string.payments_add_phone_number;
                break;
            case CompletionStatus.INVALID_ADDRESS:
                editMessageResId = R.string.payments_invalid_address;
                editTitleResId = R.string.payments_add_valid_address;
                break;
            default:
                // Multiple bits are set.
                assert completionStatus < CompletionStatus.MAX_VALUE : "Invalid completion status";
                editMessageResId = R.string.payments_more_information_required;
                editTitleResId = R.string.payments_add_more_information;
        }

        return new Pair<Integer, Integer>(editMessageResId, editTitleResId);
    }

    /**
     * Checks address completion status in the given profile.
     *
     * If the country code is not set or invalid, but all fields for the default locale's country
     * code are present, then the profile is deemed "complete." AutoflllAddress.toPaymentAddress()
     * will use the default locale to fill in a blank country code before sending the address to the
     * renderer.
     *
     * @param profile   The autofill profile containing the address information.
     * @return int      The completion status.
     */
    @CompletionStatus
    public static int checkAddressCompletionStatus(AutofillProfile profile) {
        @CompletionStatus
        int completionStatus = CompletionStatus.COMPLETE;

        if (TextUtils.isEmpty(profile.getFullName())) {
            completionStatus |= CompletionStatus.INVALID_RECIPIENT;
        }

        // TODO(crbug.com/999286): PhoneNumberUtils internally trigger disk reads for certain
        //                         devices/configurations.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!PhoneNumberUtils.isGlobalPhoneNumber(
                        PhoneNumberUtils.stripSeparators(profile.getPhoneNumber().toString()))) {
                completionStatus |= CompletionStatus.INVALID_PHONE_NUMBER;
            }
        }

        List<Integer> requiredFields = AutofillProfileBridge.getRequiredAddressFields(
                AutofillAddress.getCountryCode(profile));
        for (int fieldId : requiredFields) {
            if (fieldId == AddressField.RECIPIENT || fieldId == AddressField.COUNTRY) continue;
            if (!TextUtils.isEmpty(getProfileField(profile, fieldId))) continue;
            completionStatus |= CompletionStatus.INVALID_ADDRESS;
            break;
        }

        return completionStatus;
    }

    /** @return The given autofill profile field. */
    public static String getProfileField(AutofillProfile profile, int field) {
        assert profile != null;
        switch (field) {
            case AddressField.COUNTRY:
                return profile.getCountryCode();
            case AddressField.ADMIN_AREA:
                return profile.getRegion();
            case AddressField.LOCALITY:
                return profile.getLocality();
            case AddressField.DEPENDENT_LOCALITY:
                return profile.getDependentLocality();
            case AddressField.SORTING_CODE:
                return profile.getSortingCode();
            case AddressField.POSTAL_CODE:
                return profile.getPostalCode();
            case AddressField.STREET_ADDRESS:
                return profile.getStreetAddress();
            case AddressField.ORGANIZATION:
                return profile.getCompanyName();
            case AddressField.RECIPIENT:
                return profile.getFullName();
        }

        assert false;
        return null;
    }

    /** @return The country code to use, e.g., when constructing an editor for this address. */
    public static String getCountryCode(@Nullable AutofillProfile profile) {
        if (sRegionCodePattern == null) sRegionCodePattern = Pattern.compile(REGION_CODE_PATTERN);

        return profile == null || TextUtils.isEmpty(profile.getCountryCode())
                        || !sRegionCodePattern.matcher(profile.getCountryCode()).matches()
                ? Locale.getDefault().getCountry() : profile.getCountryCode();
    }

    /** @return The address for the merchant. */
    public PaymentAddress toPaymentAddress() {
        assert mIsComplete;
        PaymentAddress result = new PaymentAddress();

        result.country = getCountryCode(mProfile);
        result.addressLine = mProfile.getStreetAddress().split("\n");
        result.region = mProfile.getRegion();
        result.city = mProfile.getLocality();
        result.dependentLocality = mProfile.getDependentLocality();
        result.postalCode = mProfile.getPostalCode();
        result.sortingCode = mProfile.getSortingCode();
        result.organization = mProfile.getCompanyName();
        result.recipient = mProfile.getFullName();
        result.phone = mProfile.getPhoneNumber();

        return result;
    }

    private int calculateCompletenessScore() {
        int missingFields = checkAddressCompletionStatus(mProfile);

        // Count how many are set. The completeness of the address is weighted so as
        // to dominate the other fields.
        int completenessScore = 0;
        if ((missingFields & CompletionStatus.INVALID_RECIPIENT) == 0) completenessScore++;
        if ((missingFields & CompletionStatus.INVALID_PHONE_NUMBER) == 0) completenessScore++;
        if ((missingFields & CompletionStatus.INVALID_ADDRESS) == 0) completenessScore += 10;
        return completenessScore;
    }
}
