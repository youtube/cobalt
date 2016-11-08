// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.example.native_activity;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import java.io.File;

public class CobaltNativeActivity extends NativeActivity {

    public static final String TAG = "native-activity";
    public static final String LIB = "native-activity";

    static {
        System.loadLibrary(LIB);
    }

    SurfaceView mSurfaceView;
    SurfaceHolder mSurfaceHolder;
    static Surface mSurface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_cobalt_native);

        mSurfaceView = (SurfaceView) findViewById(R.id.surfaceview);
        mSurfaceHolder = mSurfaceView.getHolder();

        mSurfaceHolder.addCallback(new SurfaceHolder.Callback() {

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.i(TAG, "surfaceChanged; format=" + format + ", width =" + width + ", height ="
                        + height);
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.i(TAG, "surfaceCreated");
                mSurface = holder.getSurface();
                onVideoSurfaceCreated(mSurface);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.i(TAG, "surfaceDestroyed");
            }

        });
    }

    public String getFilePath() {
        File f = getExternalFilesDir(null);
        if(f != null) {
            return f.getAbsolutePath();
        }
        return "";
    }

    public String getCachePath() {
        File f = getExternalCacheDir();
        if(f != null) {
            return f.getAbsolutePath();
        }
        return "";
    }

    /* Override back button
     * TODO: On home screen, back should actually exit */
    public void onBackPressed() {
        Log.i(TAG, "User pressed BACK\n");
    }

    /* Use immersive full-screen mode */
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        mSurfaceView.setSystemUiVisibility((
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY));
    }

    /* Pass video surface to native side */
    public native void onVideoSurfaceCreated(Surface surface);
}
