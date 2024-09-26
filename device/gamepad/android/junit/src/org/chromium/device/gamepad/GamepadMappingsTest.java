// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThan;

import android.os.Build;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Arrays;
import java.util.BitSet;

/**
 * Verify no regressions in gamepad mappings.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GamepadMappingsTest {
    private static final float ERROR_TOLERANCE = 0.000001f;
    /**
     * Product ID for Xbox One S gamepads with updated firmware connected over Bluetooth.
     * Microsoft released a firmware update for this gamepad that changes the button and axis
     * assignments. We distinguish between them by comparing the product ID.
     */
    private static final int XBOX_ONE_S_PRODUCT_ID = 0x02fd;
    /**
     * The device ID string for Xbox One S gamepads connected over Bluetooth.
     */
    private static final String XBOX_WIRELESS_DEVICE_NAME = "Xbox Wireless Controller";
    /**
     * Set bits indicate that we don't expect the button at mMappedButtons[index] to be mapped.
     */
    private BitSet mUnmappedButtons = new BitSet(CanonicalButtonIndex.COUNT);
    /**
     * Set bits indicate that we don't expect the axis at mMappedAxes[index] to be mapped.
     */
    private BitSet mUnmappedAxes = new BitSet(CanonicalAxisIndex.COUNT);
    private float[] mMappedButtons = new float[CanonicalButtonIndex.COUNT + 2];
    private float[] mMappedAxes = new float[CanonicalAxisIndex.COUNT];
    private float[] mRawButtons = new float[GamepadDevice.MAX_RAW_BUTTON_VALUES];
    private float[] mRawAxes = new float[GamepadDevice.MAX_RAW_AXIS_VALUES];

    @Before
    public void setUp() {
        // By default, we expect every button and axis to be mapped.
        mUnmappedButtons.clear();
        mUnmappedAxes.clear();

        // Start with all the mapped values as unmapped.
        Arrays.fill(mMappedButtons, Float.NaN);
        Arrays.fill(mMappedAxes, Float.NaN);

        // Set each raw value to something unique.
        for (int i = 0; i < GamepadDevice.MAX_RAW_AXIS_VALUES; i++) {
            mRawAxes[i] = -i - 1.0f;
        }
        for (int i = 0; i < GamepadDevice.MAX_RAW_BUTTON_VALUES; i++) {
            mRawButtons[i] = i + 1.0f;
        }
    }

    @Test
    @Feature({"Gamepad"})
    public void testShieldGamepadMappings() {
        GamepadMappings mappings =
                GamepadMappings.getMappings(GamepadMappings.NVIDIA_SHIELD_DEVICE_NAME_PREFIX);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertShieldGamepadMappings(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testXBox360GamepadMappings() {
        GamepadMappings mappings =
                GamepadMappings.getMappings(GamepadMappings.MICROSOFT_XBOX_PAD_DEVICE_NAME);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertShieldGamepadMappings(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testPS3SixAxisGamepadMappings() {
        GamepadMappings mappings =
                GamepadMappings.getMappings(GamepadMappings.PS_DUALSHOCK_3_SIXAXIS_DEVICE_NAME);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Changes in Android 9 caused the DualShock 3 and SIXAXIS mappings to change.
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);
        } else {
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
        }

        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonDpadButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxesToBottomShoulder();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testSamsungEIGP20GamepadMappings() {
        GamepadMappings mappings =
                GamepadMappings.getMappings(GamepadMappings.SAMSUNG_EI_GP20_DEVICE_NAME);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedUpperTriggerButtonsToBottomShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();

        expectNoShoulderButtons();
        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testAmazonFireGamepadMappings() {
        GamepadMappings mappings =
                GamepadMappings.getMappings(GamepadMappings.AMAZON_FIRE_DEVICE_NAME);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedPedalAxesToBottomShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testUnknownXBox360GamepadMappings() {
        int[] axes = new int[] {
            MotionEvent.AXIS_X,
            MotionEvent.AXIS_Y,
            MotionEvent.AXIS_Z,
            MotionEvent.AXIS_RZ,
            MotionEvent.AXIS_LTRIGGER,
            MotionEvent.AXIS_RTRIGGER,
            MotionEvent.AXIS_HAT_X,
            MotionEvent.AXIS_HAT_Y
        };
        BitSet buttons = new BitSet();
        buttons.set(KeyEvent.KEYCODE_BUTTON_A);
        buttons.set(KeyEvent.KEYCODE_BUTTON_B);
        buttons.set(KeyEvent.KEYCODE_BUTTON_X);
        buttons.set(KeyEvent.KEYCODE_BUTTON_Y);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBL);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBR);
        buttons.set(KeyEvent.KEYCODE_BUTTON_START);
        buttons.set(KeyEvent.KEYCODE_BUTTON_SELECT);
        buttons.set(KeyEvent.KEYCODE_BUTTON_MODE);

        GamepadMappings mappings = GamepadMappings.getUnknownGamepadMappings(axes, buttons);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxesToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testUnknownMogaProGamepadMappings() {
        int[] axes = new int[] {
            MotionEvent.AXIS_X,
            MotionEvent.AXIS_Y,
            MotionEvent.AXIS_Z,
            MotionEvent.AXIS_RZ,
            MotionEvent.AXIS_BRAKE,
            MotionEvent.AXIS_GAS,
            MotionEvent.AXIS_HAT_X,
            MotionEvent.AXIS_HAT_Y
        };
        BitSet buttons = new BitSet();
        buttons.set(KeyEvent.KEYCODE_BUTTON_A);
        buttons.set(KeyEvent.KEYCODE_BUTTON_B);
        buttons.set(KeyEvent.KEYCODE_BUTTON_X);
        buttons.set(KeyEvent.KEYCODE_BUTTON_Y);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBL);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBR);
        buttons.set(KeyEvent.KEYCODE_BUTTON_START);
        buttons.set(KeyEvent.KEYCODE_BUTTON_SELECT);
        buttons.set(KeyEvent.KEYCODE_BUTTON_MODE);

        GamepadMappings mappings = GamepadMappings.getUnknownGamepadMappings(axes, buttons);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedPedalAxesToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testUnknownXiaomiGamepadMappings() {
        int[] axes = new int[] {
            MotionEvent.AXIS_X,
            MotionEvent.AXIS_Y,
            MotionEvent.AXIS_RX,
            MotionEvent.AXIS_RY,
            MotionEvent.AXIS_BRAKE,
            MotionEvent.AXIS_THROTTLE,
            MotionEvent.AXIS_HAT_X,
            MotionEvent.AXIS_HAT_Y
        };
        BitSet buttons = new BitSet();
        buttons.set(KeyEvent.KEYCODE_BUTTON_A);
        buttons.set(KeyEvent.KEYCODE_BUTTON_B);
        buttons.set(KeyEvent.KEYCODE_BUTTON_X);
        buttons.set(KeyEvent.KEYCODE_BUTTON_Y);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBL);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBR);
        buttons.set(KeyEvent.KEYCODE_BUTTON_START);
        buttons.set(KeyEvent.KEYCODE_BUTTON_SELECT);
        buttons.set(KeyEvent.KEYCODE_BUTTON_MODE);

        GamepadMappings mappings = GamepadMappings.getUnknownGamepadMappings(axes, buttons);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedAltPedalAxesToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testUnknownGpdXdGamepadMappings() {
        int[] axes = new int[] {
            MotionEvent.AXIS_X,
            MotionEvent.AXIS_Y,
            MotionEvent.AXIS_Z,
            MotionEvent.AXIS_RZ
        };
        BitSet buttons = new BitSet();
        buttons.set(KeyEvent.KEYCODE_BUTTON_A);
        buttons.set(KeyEvent.KEYCODE_BUTTON_B);
        buttons.set(KeyEvent.KEYCODE_BUTTON_X);
        buttons.set(KeyEvent.KEYCODE_BUTTON_Y);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L2);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R2);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBL);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBR);
        buttons.set(KeyEvent.KEYCODE_BUTTON_START);
        buttons.set(KeyEvent.KEYCODE_BUTTON_SELECT);
        buttons.set(KeyEvent.KEYCODE_BUTTON_MODE);

        GamepadMappings mappings = GamepadMappings.getUnknownGamepadMappings(axes, buttons);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedLowerTriggerButtonsToBottomShoulder();
        assertMappedCommonDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testUnknownGamepadMappingsNoMeta() {
        int[] axes = new int[] {
                MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ};
        BitSet buttons = new BitSet();
        buttons.set(KeyEvent.KEYCODE_BUTTON_A);
        buttons.set(KeyEvent.KEYCODE_BUTTON_B);
        buttons.set(KeyEvent.KEYCODE_BUTTON_X);
        buttons.set(KeyEvent.KEYCODE_BUTTON_Y);
        buttons.set(KeyEvent.KEYCODE_BUTTON_L1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_R1);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBL);
        buttons.set(KeyEvent.KEYCODE_BUTTON_THUMBR);
        buttons.set(KeyEvent.KEYCODE_BUTTON_START);
        buttons.set(KeyEvent.KEYCODE_BUTTON_SELECT);

        mRawButtons[KeyEvent.KEYCODE_BUTTON_MODE] = 0.0f;
        GamepadMappings mappings = GamepadMappings.getUnknownGamepadMappings(axes, buttons);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        expectNoMetaButton(mappings);

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testPS4GamepadMappings() {
        int[] axes;
        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            axes = new int[] {MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z,
                    MotionEvent.AXIS_RZ, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER,
                    MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y};
        } else {
            axes = new int[] {MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z,
                    MotionEvent.AXIS_RX, MotionEvent.AXIS_RY, MotionEvent.AXIS_RZ,
                    MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y};
        }
        GamepadMappings mappings = GamepadMappings.getMappings(
                GamepadMappings.SONY_VENDOR_ID, GamepadMappings.PS_DUALSHOCK_4_PRODUCT_ID, axes);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Changes in Android 9 caused the DualShock 4 mapping to change.
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);

            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_L1], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_R1], ERROR_TOLERANCE);

            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                    mRawAxes[MotionEvent.AXIS_LTRIGGER], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                    mRawAxes[MotionEvent.AXIS_RTRIGGER], ERROR_TOLERANCE);

            assertMappedCommonThumbstickButtons();
        } else {
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_C], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);

            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_Z], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                    mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);

            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                    mRawAxes[MotionEvent.AXIS_RX], ERROR_TOLERANCE);
            Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                    mRawAxes[MotionEvent.AXIS_RY], ERROR_TOLERANCE);

            expectNoThumbstickButtons();
        }

        assertMappedCommonStartSelectMetaButtons();
        assertMappedXYAxes();
        assertMappedHatAxisToDpadButtons();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testXboxOneSBluetooth2016FirmwareMappings() {
        int[] axes = new int[] {MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RZ, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER,
                MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y};
        GamepadMappings mappings = GamepadMappings.getMappings(GamepadMappings.MICROSOFT_VENDOR_ID,
                GamepadMappings.XBOX_ONE_S_2016_FIRMWARE_PRODUCT_ID, axes);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_C], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);

        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_Z], ERROR_TOLERANCE);

        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L2], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R2], ERROR_TOLERANCE);

        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.BACK_SELECT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L1], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.START],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R1], ERROR_TOLERANCE);

        // The triggers range from -1 to 1 with -1 as the idle value.
        float leftTriggerValue = (mRawAxes[MotionEvent.AXIS_Z] + 1.0f) / 2.0f;
        float rightTriggerValue = (mRawAxes[MotionEvent.AXIS_RZ] + 1.0f) / 2.0f;
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER], leftTriggerValue,
                ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER], rightTriggerValue,
                ERROR_TOLERANCE);

        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();
        expectNoMetaButton(mappings);

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testXboxOneSBluetoothUsesDefaultMappings() {
        // Test that Xbox One S gamepads with updated firmware connected over Bluetooth use the
        // default mapping.
        int[] axes = new int[] {MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RZ, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER,
                MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y};
        GamepadMappings deviceIdMappings = GamepadMappings.getMappings(
                GamepadMappings.MICROSOFT_VENDOR_ID, XBOX_ONE_S_PRODUCT_ID, axes);
        Assert.assertNull(deviceIdMappings);

        GamepadMappings deviceNameMappings = GamepadMappings.getMappings(XBOX_WIRELESS_DEVICE_NAME);
        Assert.assertNull(deviceNameMappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testIDroidConGamepadMappingsDigital() {
        int[] axes = {
                MotionEvent.AXIS_X,
                MotionEvent.AXIS_Y,
                MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RZ,
                MotionEvent.AXIS_HAT_X,
                MotionEvent.AXIS_HAT_Y,
        };
        GamepadMappings mappings = GamepadMappings.getMappings(GamepadMappings.BROADCOM_VENDOR_ID,
                GamepadMappings.SNAKEBYTE_IDROIDCON_PRODUCT_ID, axes);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        expectNoMetaButton(mappings);
        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedLowerTriggerButtonsToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.START],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_START], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.BACK_SELECT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_SELECT], ERROR_TOLERANCE);

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testIDroidConGamepadMappingsAnalog() {
        int[] axes = {
                MotionEvent.AXIS_X,
                MotionEvent.AXIS_Y,
                MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RX,
                MotionEvent.AXIS_RY,
                MotionEvent.AXIS_HAT_X,
                MotionEvent.AXIS_HAT_Y,
        };
        GamepadMappings mappings = GamepadMappings.getMappings(GamepadMappings.BROADCOM_VENDOR_ID,
                GamepadMappings.SNAKEBYTE_IDROIDCON_PRODUCT_ID, axes);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        expectNoMetaButton(mappings);
        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedRXAndRYAxesToRightStick();
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.START],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_START], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.BACK_SELECT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_SELECT], ERROR_TOLERANCE);
        Assert.assertEquals(
                mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER], 0.0, ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                -mRawAxes[MotionEvent.AXIS_Z], ERROR_TOLERANCE);

        assertMapping(mappings);
    }

    @Test
    @Feature({"Gamepad"})
    public void testStadiaControllerMappings() {
        int[] axes = {
                MotionEvent.AXIS_X,
                MotionEvent.AXIS_Y,
                MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RZ,
                MotionEvent.AXIS_HAT_X,
                MotionEvent.AXIS_HAT_Y,
                MotionEvent.AXIS_GAS,
                MotionEvent.AXIS_BRAKE,
        };
        GamepadMappings mappings = GamepadMappings.getMappings(GamepadMappings.GOOGLE_VENDOR_ID,
                GamepadMappings.STADIA_CONTROLLER_PRODUCT_ID, axes);
        mappings.mapToStandardGamepad(mMappedAxes, mMappedButtons, mRawAxes, mRawButtons);

        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedPedalAxesToBottomShoulder();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedCommonThumbstickButtons();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        // The Assistant and Capture buttons should be mapped after the last
        // Standard Gamepad button index.
        Assert.assertEquals(mappings.getButtonsLength(), CanonicalButtonIndex.COUNT + 2);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.COUNT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_1], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.COUNT + 1],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_2], ERROR_TOLERANCE);

        assertMapping(mappings);
    }

    /**
     * Asserts that the current gamepad mapping being tested matches the shield mappings.
     */
    public void assertShieldGamepadMappings(GamepadMappings mappings) {
        assertMappedCommonXYABButtons();
        assertMappedTriggerButtonsToTopShoulder();
        assertMappedCommonThumbstickButtons();
        assertMappedCommonStartSelectMetaButtons();
        assertMappedTriggerAxesToBottomShoulder();
        assertMappedHatAxisToDpadButtons();
        assertMappedXYAxes();
        assertMappedZAndRZAxesToRightStick();

        assertMapping(mappings);
    }

    public void expectNoShoulderButtons() {
        mUnmappedButtons.set(CanonicalButtonIndex.LEFT_SHOULDER);
        mUnmappedButtons.set(CanonicalButtonIndex.RIGHT_SHOULDER);
    }

    public void expectNoMetaButton(GamepadMappings mappings) {
        mUnmappedButtons.set(CanonicalButtonIndex.META);
        assertThat(mappings.getButtonsLength(), lessThan(CanonicalButtonIndex.COUNT));
    }

    public void expectNoThumbstickButtons() {
        mUnmappedButtons.set(CanonicalButtonIndex.LEFT_THUMBSTICK);
        mUnmappedButtons.set(CanonicalButtonIndex.RIGHT_THUMBSTICK);
    }

    public void assertMapping(GamepadMappings mappings) {
        for (int i = 0; i < mMappedAxes.length; i++) {
            if (mUnmappedAxes.get(i)) {
                Assert.assertTrue(
                        "An unexpected axis was mapped at index " + i, Float.isNaN(mMappedAxes[i]));
            } else {
                Assert.assertFalse(
                        "An axis was not mapped at index " + i, Float.isNaN(mMappedAxes[i]));
            }
        }
        assertThat(mMappedButtons.length, greaterThanOrEqualTo(mappings.getButtonsLength()));
        for (int i = 0; i < mappings.getButtonsLength(); i++) {
            if (mUnmappedButtons.get(i)) {
                Assert.assertTrue("An unexpected button was mapped at index " + i,
                        Float.isNaN(mMappedButtons[i]));
            } else {
                Assert.assertFalse(
                        "A button was not mapped at index " + i, Float.isNaN(mMappedButtons[i]));
            }
        }
    }

    private void assertMappedUpperTriggerButtonsToBottomShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L1], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R1], ERROR_TOLERANCE);
    }

    private void assertMappedLowerTriggerButtonsToBottomShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L2], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R2], ERROR_TOLERANCE);
    }

    private void assertMappedCommonDpadButtons() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_DOWN],
                mRawButtons[KeyEvent.KEYCODE_DPAD_DOWN], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_UP],
                mRawButtons[KeyEvent.KEYCODE_DPAD_UP], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_LEFT],
                mRawButtons[KeyEvent.KEYCODE_DPAD_LEFT], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_RIGHT],
                mRawButtons[KeyEvent.KEYCODE_DPAD_RIGHT], ERROR_TOLERANCE);
    }

    private void assertMappedTriggerButtonsToTopShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_L1], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_R1], ERROR_TOLERANCE);
    }

    private void assertMappedCommonXYABButtons() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.PRIMARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_A], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.SECONDARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_B], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.TERTIARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_X], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.QUATERNARY],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_Y], ERROR_TOLERANCE);
    }

    private void assertMappedCommonThumbstickButtons() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR], ERROR_TOLERANCE);
    }

    private void assertMappedCommonStartSelectMetaButtons() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.START],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_START], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.BACK_SELECT],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_SELECT], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.META],
                mRawButtons[KeyEvent.KEYCODE_BUTTON_MODE], ERROR_TOLERANCE);
    }

    private void assertMappedPedalAxesToBottomShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_BRAKE], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_GAS], ERROR_TOLERANCE);
    }

    private void assertMappedAltPedalAxesToBottomShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_BRAKE], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_THROTTLE], ERROR_TOLERANCE);
    }

    private void assertMappedTriggerAxesToBottomShoulder() {
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.LEFT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_LTRIGGER], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER],
                mRawAxes[MotionEvent.AXIS_RTRIGGER], ERROR_TOLERANCE);
    }

    private void assertMappedHatAxisToDpadButtons() {
        float hatX = mRawAxes[MotionEvent.AXIS_HAT_X];
        float hatY = mRawAxes[MotionEvent.AXIS_HAT_Y];
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_LEFT],
                GamepadMappings.negativeAxisValueAsButton(hatX), ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_RIGHT],
                GamepadMappings.positiveAxisValueAsButton(hatX), ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_UP],
                GamepadMappings.negativeAxisValueAsButton(hatY), ERROR_TOLERANCE);
        Assert.assertEquals(mMappedButtons[CanonicalButtonIndex.DPAD_DOWN],
                GamepadMappings.positiveAxisValueAsButton(hatY), ERROR_TOLERANCE);
    }

    private void assertMappedXYAxes() {
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.LEFT_STICK_X],
                mRawAxes[MotionEvent.AXIS_X], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.LEFT_STICK_Y],
                mRawAxes[MotionEvent.AXIS_Y], ERROR_TOLERANCE);
    }

    private void assertMappedRXAndRYAxesToRightStick() {
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_X],
                mRawAxes[MotionEvent.AXIS_RX], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y],
                mRawAxes[MotionEvent.AXIS_RY], ERROR_TOLERANCE);
    }

    private void assertMappedZAndRZAxesToRightStick() {
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_X],
                mRawAxes[MotionEvent.AXIS_Z], ERROR_TOLERANCE);
        Assert.assertEquals(mMappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y],
                mRawAxes[MotionEvent.AXIS_RZ], ERROR_TOLERANCE);
    }
}
