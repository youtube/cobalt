// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Dialog;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** Tests for the UsbChooserDialog class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(BluetoothChooserDialogTest.DEVICE_DIALOG_BATCH_NAME)
public class UsbChooserDialogTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public JniMocker mocker = new JniMocker();

    private String mSelectedDeviceId = "";

    private UsbChooserDialog mChooserDialog;

    private class TestUsbChooserDialogJni implements UsbChooserDialog.Natives {
        @Override
        public void onItemSelected(long nativeUsbChooserDialogAndroid, String deviceId) {
            mSelectedDeviceId = deviceId;
        }

        @Override
        public void onDialogCancelled(long nativeUsbChooserDialogAndroid) {}

        @Override
        public void loadUsbHelpPage(long nativeUsbChooserDialogAndroid) {}
    }

    @Before
    public void setUp() throws Exception {
        mocker.mock(UsbChooserDialogJni.TEST_HOOKS, new TestUsbChooserDialogJni());
        mChooserDialog = createDialog();
    }

    private UsbChooserDialog createDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    UsbChooserDialog dialog =
                            new UsbChooserDialog(
                                    /* nativeUsbChooserDialogPtr= */ 42,
                                    Profile.getLastUsedRegularProfile());
                    dialog.show(
                            sActivityTestRule.getActivity(),
                            "https://origin.example.com/",
                            ConnectionSecurityLevel.SECURE);
                    return dialog;
                });
    }

    private void selectItem(int position) {
        final Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(items.getChildAt(0), Matchers.notNullValue()));

        // The actual index for the first item displayed on screen.
        int firstVisiblePosition = items.getFirstVisiblePosition();
        TouchCommon.singleClickView(items.getChildAt(position - firstVisiblePosition));

        CriteriaHelper.pollUiThread(() -> button.isEnabled());
        // Make sure the button is properly rendered before clicking.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(button.getHeight(), Matchers.greaterThan(0));
                });
        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mSelectedDeviceId, Matchers.not(Matchers.isEmptyString()));
                });
    }

    /**
     * The messages include <link> ... </link> or <link1> ... </link1>, <link2> ... </link2>
     * sections that are used to create clickable spans. For testing the messages, this function
     * returns the raw string without the tags.
     */
    private static String removeLinkTags(String message) {
        return message.replaceAll("</?link1>", "")
                .replaceAll("</?link2>", "")
                .replaceAll("</?link>", "");
    }

    @Test
    @SmallTest
    public void testCancel() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // The 'Connect' button should be disabled and the list view should be hidden.
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        dialog.cancel();

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mSelectedDeviceId, Matchers.isEmptyString()));
    }

    @Test
    @SmallTest
    public void testSelectItem() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final int position = 1;

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mChooserDialog.addDevice("device_id_0", "device_name_0");
                    mChooserDialog.addDevice("device_id_1", "device_name_1");
                    mChooserDialog.addDevice("device_id_2", "device_name_2");
                    // Show the desired position at the top of the ListView (in case
                    // not all the items can fit on small devices' screens).
                    items.setSelection(position);
                });

        // After adding items to the dialog, the help message should be showing,
        // the 'Connect' button should still be disabled (since nothing's selected),
        // and the list view should show.
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule
                                .getActivity()
                                .getString(R.string.usb_chooser_dialog_footnote_text)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, items.getVisibility());

        selectItem(position);

        Assert.assertEquals("device_id_1", mSelectedDeviceId);
    }
}
