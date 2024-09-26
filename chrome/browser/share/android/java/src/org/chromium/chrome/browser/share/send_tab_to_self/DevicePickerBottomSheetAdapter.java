// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.widget.ChromeImageView;

import java.util.Calendar;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Adapter to populate the Target Device Picker sheet.
 */
public class DevicePickerBottomSheetAdapter extends BaseAdapter {
    private final List<TargetDeviceInfo> mTargetDevices;

    public DevicePickerBottomSheetAdapter(List<TargetDeviceInfo> targetDevices) {
        mTargetDevices = targetDevices;
    }

    @Override
    public int getCount() {
        return mTargetDevices.size();
    }

    @Override
    public TargetDeviceInfo getItem(int position) {
        return mTargetDevices.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            final Context context = parent.getContext();
            convertView = LayoutInflater.from(context).inflate(
                    R.layout.send_tab_to_self_device_picker_item, parent, false);

            TargetDeviceInfo deviceInfo = getItem(position);
            ChromeImageView deviceIcon = convertView.findViewById(R.id.device_icon);
            deviceIcon.setImageDrawable(getDrawableForDeviceType(context, deviceInfo));
            deviceIcon.setVisibility(View.VISIBLE);

            TextView deviceName = convertView.findViewById(R.id.device_name);
            deviceName.setText(deviceInfo.deviceName);

            TextView lastActive = convertView.findViewById(R.id.last_active);

            long numDaysDeviceActive = TimeUnit.MILLISECONDS.toDays(
                    Calendar.getInstance().getTimeInMillis() - deviceInfo.lastUpdatedTimestamp);
            lastActive.setText(getLastActiveMessage(context.getResources(), numDaysDeviceActive));
        }
        return convertView;
    }

    private static String getLastActiveMessage(Resources resources, long numDays) {
        if (numDays < 1) {
            return resources.getString(R.string.send_tab_to_self_device_last_active_today);
        } else if (numDays == 1) {
            return resources.getString(R.string.send_tab_to_self_device_last_active_one_day_ago);
        } else {
            return resources.getString(
                    R.string.send_tab_to_self_device_last_active_more_than_one_day, numDays);
        }
    }

    private static Drawable getDrawableForDeviceType(
            Context context, TargetDeviceInfo targetDevice) {
        // TODO(crbug.com/1368080): Update cases to handle a tablet device case.
        switch (targetDevice.formFactor) {
            case FormFactor.DESKTOP: {
                return AppCompatResources.getDrawable(context, R.drawable.computer_black_24dp);
            }
            case FormFactor.PHONE: {
                return AppCompatResources.getDrawable(context, R.drawable.smartphone_black_24dp);
            }
        }
        return AppCompatResources.getDrawable(context, R.drawable.devices_black_24dp);
    }
}
