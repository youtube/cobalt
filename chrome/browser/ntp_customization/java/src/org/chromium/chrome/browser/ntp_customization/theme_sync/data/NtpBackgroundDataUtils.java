// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.BACKGROUND_TYPE_KEY;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;

/** Utility class for NTP background data conversion. */
@NullMarked
public class NtpBackgroundDataUtils {
    private static final String TAG = "NtpThemeDataUtils";
    private static final String[] sProjection = {
        OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE
    };

    /**
     * Returns the NtpBackgroundDataBase object from the given JSON representation. Null if the type
     * is invalid.
     */
    public static @Nullable NtpBackgroundDataBase fromJson(Context context, JSONObject jsonObject)
            throws JSONException {
        int backgroundType = jsonObject.getInt(BACKGROUND_TYPE_KEY);
        switch (backgroundType) {
            case NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR:
                return NtpBackgroundDataColor.fromJson(context, jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.COLOR_FROM_HEX:
                return NtpBackgroundDataCustomizedColor.fromJson(context, jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK:
                return NtpBackgroundDataUploadImage.fromJson(jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION:
                return NtpBackgroundDataThemeCollection.fromJson(jsonObject);
            default:
                return null;
        }
    }

    /**
     * Converts a {@link JSONArray} to a {@link Matrix}.
     *
     * @param jsonArray The {@link JSONArray} to convert.
     * @return The {@link Matrix} represented by the {@link JSONArray}.
     */
    public static @Nullable Matrix jsonArrayToMatrix(@Nullable JSONArray jsonArray) {
        if (jsonArray == null) return null;

        try {
            float[] values = new float[9];
            for (int i = 0; i < 9; i++) {
                values[i] = (float) jsonArray.getDouble(i);
            }
            Matrix matrix = new Matrix();
            matrix.setValues(values);
            return matrix;
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * Converts a {@link Matrix} to a {@link JSONArray}.
     *
     * @param matrix The {@link Matrix} to convert.
     * @return The {@link JSONArray} representation of the {@link Matrix}.
     */
    public static @Nullable JSONArray matrixToJsonArray(@Nullable Matrix matrix) {
        if (matrix == null) return null;

        try {
            float[] values = new float[9];
            matrix.getValues(values);
            JSONArray jsonArray = new JSONArray();
            for (float v : values) {
                jsonArray.put(v);
            }
            return jsonArray;
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * Converts a {@link Point} to a {@link JSONArray}.
     *
     * @param point The {@link Point} to convert.
     * @return The {@link JSONArray} representation of the {@link Point}, or null if the input is
     *     null.
     */
    public static @Nullable JSONArray pointToJsonArray(@Nullable Point point) {
        if (point == null) return null;

        JSONArray jsonArray = new JSONArray();
        jsonArray.put(point.x);
        jsonArray.put(point.y);
        return jsonArray;
    }

    /**
     * Converts a {@link JSONArray} to a {@link Point}.
     *
     * @param jsonArray The {@link JSONArray} to convert.
     * @return The {@link Point} represented by the {@link JSONArray}, or null if the input is null
     *     or invalid.
     */
    public static @Nullable Point jsonArrayToPoint(@Nullable JSONArray jsonArray) {
        if (jsonArray == null) return null;

        try {
            if (jsonArray.length() != 2) {
                return null;
            }
            return new Point(jsonArray.getInt(0), jsonArray.getInt(1));
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * Loads the NTP background image from disk asynchronously.
     *
     * @param onImageLoadedCallback The callback to invoke when the image is loaded.
     */
    static void loadImage(Callback<@Nullable Bitmap> onImageLoadedCallback) {
        NtpCustomizationUtils.readNtpBackgroundImage(
                (bitmap) -> {
                    onImageLoadedCallback.onResult(bitmap);
                },
                NtpCustomizationConfigManager.EXECUTOR);
    }

    /**
     * Generates an instant metadata-based unique key for a Photo Picker URI. Safe to run on the
     * Main UI thread.
     *
     * @return A string identity key like "1000003842.jpg_3421055"
     */
    public static String getMetadataFingerprint(Context context, Uri uri) {
        if (uri == null) return "";

        String displayName = "";
        long fileSize = -1;

        // Query the ContentResolver using try-with-resources to auto-close the cursor.
        try (Cursor cursor =
                context.getContentResolver().query(uri, sProjection, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);

                if (nameIndex != -1) {
                    displayName = cursor.getString(nameIndex);
                }
                if (sizeIndex != -1) {
                    fileSize = cursor.getLong(sizeIndex);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to resolve content metadata for the given Uri.", e);
        }

        // Safety fallback: if the system provider fails to yield a name,
        // fall back to using the last path segment of the wrapper URI itself.
        if (displayName == null || displayName.isEmpty()) {
            displayName = uri.getLastPathSegment();
        }

        // Return a combined string key.
        return displayName + "_" + fileSize;
    }
}
