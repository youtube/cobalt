// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForS;

import java.util.ArrayList;
import java.util.List;

@RequiresApi(Build.VERSION_CODES.S)
class AudioDeviceSelectorPostS extends AudioDeviceSelector {
    private static final String TAG = "mediaTHORTHORTHOR";

    private boolean mHasBluetoothConnectPermission;

    public AudioDeviceSelectorPostS(AudioManager audioManager) {
        super(audioManager);
    }

    private static List<Integer> getTargetTypesFromId(int deviceId) {
        List<Integer> types = new ArrayList<Integer>();

        Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - GET R TARGET TUPE FOR DEVICID:" + deviceId) ;

        switch (deviceId) {
            case Devices.ID_SPEAKERPHONE:
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - SPKR PHONE HEADSEt");
                types.add(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER);
                break;
            case Devices.ID_WIRED_HEADSET:
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - WIRED HEADSEt");
                types.add(AudioDeviceInfo.TYPE_WIRED_HEADSET);
                types.add(AudioDeviceInfo.TYPE_WIRED_HEADPHONES);
                break;
            case Devices.ID_EARPIECE:
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - EARPIECE");
                types.add(AudioDeviceInfo.TYPE_BUILTIN_EARPIECE);
                break;
            case Devices.ID_BLUETOOTH_HEADSET:
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - BLJUETOORH HEADSET");
                types.add(AudioDeviceInfo.TYPE_BLUETOOTH_SCO);
                types.add(AudioDeviceInfo.TYPE_BLUETOOTH_A2DP);
                break;
            case Devices.ID_USB_AUDIO:
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - USB AUDIO");
                types.add(AudioDeviceInfo.TYPE_USB_HEADSET);
                types.add(AudioDeviceInfo.TYPE_USB_DEVICE);
                break;
        }

        return types;
    }

    @Override
    public void init() {
        mHasBluetoothConnectPermission = ApiHelperForS.hasBluetoothConnectPermission();
        Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS INIT");

        if (!mHasBluetoothConnectPermission) {
            Log.i(TAG, "YO THOR _ BLUETOOTH_CONNECT permission is missing.");
        }

        mDeviceListener.init(mHasBluetoothConnectPermission);
    }

    @Override
    public void close() {
        mDeviceListener.close();
    }

    @Override
    public void setCommunicationAudioModeOn(boolean on) {
        if (on) {
          Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS setCommunicationAudioModeOn ON - PROMOT FOR BLUETOOTH_COBBECT");
            // TODO(crbug.com/1317548): Prompt for BLUETOOTH_CONNECT permission at this point if we
            // don't have it.
        } else {
            mDeviceStates.clearRequestedDevice();
            mAudioManager.clearCommunicationDevice();
        }
    }

    @Override
    public boolean isSpeakerphoneOn() {
        AudioDeviceInfo currentDevice = mAudioManager.getCommunicationDevice();
        return currentDevice != null
                && currentDevice.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
    }

    @Override
    public void setSpeakerphoneOn(boolean on) {
        boolean isCurrentlyOn = isSpeakerphoneOn();

        if (isCurrentlyOn == on) return;

        if (on) {
            setAudioDevice(Devices.ID_SPEAKERPHONE);
        } else {
            // Turn speakerphone OFF.
            mAudioManager.clearCommunicationDevice();
            maybeUpdateSelectedDevice();
        }
    }

    @Override
    public boolean[] getAvailableDevices_Locked() {

        Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS GET AVAIL DEVICS - we have this many devices:" + Devices.DEVICE_COUNT);
        List<AudioDeviceInfo> communicationDevices =
                mAudioManager.getAvailableCommunicationDevices();

        Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS - we have this many AVAIL COMMS devices:" + communicationDevices.size());
        boolean[] availableDevices = new boolean[Devices.DEVICE_COUNT];

        for (AudioDeviceInfo device : communicationDevices) {
            int type = device.getType();
            String name = device.getProductName().toString(); // Convert CharSequence to String
            boolean isOutput = device.isSink(); // Check if it's an output device
            boolean isInput = device.isSource(); // Check if it's an output device

            // Process the device information
            Log.i(TAG, "YO THOR - AudioDevice INFO"+ " ID:" +  device.getId() + " Type: " + type + ", Name: " + name + ", Output: " + isOutput +  " , Input:" + isInput);


            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                    availableDevices[Devices.ID_SPEAKERPHONE] = true;
                    break;

                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                    availableDevices[Devices.ID_WIRED_HEADSET] = true;
                    break;

                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                    availableDevices[Devices.ID_USB_AUDIO] = true;
                    break;

                case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                    Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS GET AVAIL DEVICS - BLE HEADET TRUE");
                    availableDevices[Devices.ID_BLUETOOTH_HEADSET] = true;
                    break;

                case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                    availableDevices[Devices.ID_EARPIECE] = true;
                    break;
            }
        }

        return availableDevices;
    }

    public AudioDeviceInfo getMatchingCommunicationDevice(List<Integer> targetTypes) {
        List<AudioDeviceInfo> availableDevices = mAudioManager.getAvailableCommunicationDevices();

        for (AudioDeviceInfo device : availableDevices) {
            if (targetTypes.contains(device.getType())) return device;
        }

        return null;
    }

    @Override
    protected void setAudioDevice(int deviceId) {
        Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS SET AUDIO DEVIFE:" + deviceId);
        if (!DeviceHelpers.isDeviceValid(deviceId)) {
          Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS NOT !!! VALID _ LETS RETURN");
          return;
        }

        AudioDeviceInfo targetDevice =
                getMatchingCommunicationDevice(getTargetTypesFromId(deviceId));

        if (targetDevice != null) {
            boolean result = mAudioManager.setCommunicationDevice(targetDevice);
            if (!result) {
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS ! ERR SETTING COMS DEV");
                loge("Error setting communication device");
            } else {
                Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS ! WE GOOD, HAVE A TARGET DEVICE:" + targetDevice.getProductName().toString() + " ID is:", targetDevice.getId());
            }
        } else {
            Log.i(TAG, "YO THOR! AudioDeviceSelectorPostS ! COULDNT FIND AVAIL DEVIC FOR");
            loge("Couldn't find available device for: " + DeviceHelpers.getDeviceName(deviceId));
        }
    }
}
