// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Instrumentation tests for AutofillLocalCardEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalCardEditorTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillLocalCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalCardEditor.class);

    private static final CreditCard SAMPLE_LOCAL_CARD =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* isCached= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_LOCAL_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* isCached= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444111111111111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 0,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState
                            .UNENROLLED_AND_ELIGIBLE,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "123");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @Test
    @MediumTest
    public void nicknameFieldEmpty_cardDoesNotHaveNickname() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString()).isEmpty();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void nicknameFieldSet_cardHasNickname() throws Exception {
        String nickname = "test nickname";
        SAMPLE_LOCAL_CARD.setNickname(nickname);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(nickname);
        // If the nickname is not modified the Done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testNicknameFieldIsShown() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testInvalidNicknameShowsErrorMessage() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));
        // Since the nickname has an error, the done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToValid() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));

        // Set the nickname to valid one.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Valid Nickname");
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToEmpty() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));

        // Clear the nickname.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(null);
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testNicknameLengthCappedAt25Characters() throws Exception {
        String veryLongNickname = "This is a very very long nickname";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(veryLongNickname);
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(veryLongNickname.substring(0, 25));
    }

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDateSpinnerAreShownWhenCvcFlagOff() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationMonth.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationYear.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationDate).isNull();
        assertThat(autofillLocalCardEditorFragment.mCvc).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDateAndSecurityCodeFieldsAreShown() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mCvc.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationMonth).isNull();
        assertThat(autofillLocalCardEditorFragment.mExpirationYear).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void securityCodeFieldSet_cardHasCvc() throws Exception {
        String cvc = "234";
        SAMPLE_LOCAL_CARD_WITH_CVC.setCvc(cvc);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mCvc.getText().toString()).isEqualTo(cvc);
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void expirationDateFieldSet_cardHasExpirationDate() throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        SAMPLE_LOCAL_CARD_WITH_CVC.setMonth(validExpirationMonth);
        SAMPLE_LOCAL_CARD_WITH_CVC.setYear(validExpirationYear);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getText().toString())
                .isEqualTo(
                        String.format(
                                "%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenInvalidDate_showsErrorMessage() throws Exception {
        String invalidExpirationMonth = "14";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", invalidExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(
                                        R.string
                                                .autofill_credit_card_editor_invalid_expiration_date));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateInPast_showsErrorMessage() throws Exception {
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_expired_card));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsCorrected_removesErrorMessage() throws Exception {
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";
        String validExpirationYear = AutofillTestHelper.nextYear();
        SAMPLE_LOCAL_CARD_WITH_CVC.setMonth(validExpirationMonth);
        SAMPLE_LOCAL_CARD_WITH_CVC.setYear(invalidPastExpirationYear);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_expired_card));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsEditedFromValidToIncomplete_disablesSaveButton()
            throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, /* expiration year */ ""));

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsEditedFromValidToEmpty_disablesSaveButton()
            throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(autofillLocalCardEditorFragment, /* date= */ "");

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void
            testExpirationDate_whenCorrectingOnlyNickname_keepsSaveButtonDisabledDueToInvalidDate()
                    throws Exception {
        String validNickname = "valid";
        String invalidNickname = "Invalid 123";
        String invalidPastExpirationYear = "2020";
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        setNicknameOnEditor(autofillLocalCardEditorFragment, validNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));
        setNicknameOnEditor(autofillLocalCardEditorFragment, invalidNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();

        setNicknameOnEditor(autofillLocalCardEditorFragment, validNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    private void setExpirationDateOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String date) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mExpirationDate.setText(date);
                    } catch (Exception e) {
                        Assert.fail("Failed to set the Expiration Date");
                    }
                });
    }

    private void setNicknameOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String nickname) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(nickname);
                    } catch (Exception e) {
                        Assert.fail("Failed to set the nickname");
                    }
                });
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenDoubleDigitMonth_returnsMonth() throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("12/23")).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenSingleDigitMonth_returnsMonthWithoutLeadingZero()
            throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("02/23")).isEqualTo("2");
    }

    @Test
    @SmallTest
    public void getExpirationYear_returnsYearWithPrefix() throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationYear("12/23")).isEqualTo("2023");
    }
}
