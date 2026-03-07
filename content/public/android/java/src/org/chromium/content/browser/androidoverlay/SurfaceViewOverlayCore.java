// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package org.chromium.content.browser.androidoverlay;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.IBinder;
import android.view.Gravity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.gfx.mojom.Rect;
import org.chromium.media.mojom.AndroidOverlayConfig;

class SurfaceViewOverlayCore implements OverlayCore {
    private static final String TAG = "SurfaceViewCore";
    private DialogOverlayCore.Host mHost;
    private SurfaceView mSurfaceView;
    private ViewGroup mParentView;
    private FrameLayout.LayoutParams mLayoutParams;
    private Callbacks mCallbacks;
    private boolean mAsPanel;

    public SurfaceViewOverlayCore() {}

    @Override
    public void initialize(
            Context context, AndroidOverlayConfig config, DialogOverlayCore.Host host, boolean asPanel) {
        mHost = host;
        mAsPanel = asPanel;

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            Log.e(TAG, "Failed to get activity from context for SurfaceView overlay");
            return;
        }

        mSurfaceView = new SurfaceView(activity);
        if (config.secure) {
            mSurfaceView.setSecure(true);
        }
        // Emulate how AndroidOverlay typically works: Punch a hole
        mSurfaceView.setBackgroundColor(android.graphics.Color.TRANSPARENT);
        
        mSurfaceView.getHolder().setFormat(PixelFormat.TRANSPARENT);

        // Initialize with MATCH_PARENT so the Android view system considers it valid
        // and creates the surface. The exact bounds will be set in layoutSurface().
        mLayoutParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        
        // If config provided a valid size initially, use it. But ensure it's not 0.
        if (config.rect.width > 0 && config.rect.height > 0) {
            mLayoutParams.width = config.rect.width;
            mLayoutParams.height = config.rect.height;
            mLayoutParams.leftMargin = config.rect.x;
            mLayoutParams.topMargin = config.rect.y;
        }
        
        mLayoutParams.gravity = Gravity.TOP | Gravity.LEFT;

        mParentView = (ViewGroup) activity.findViewById(android.R.id.content);
        if (mParentView == null) {
            mParentView = (ViewGroup) activity.getWindow().getDecorView();
        }
        
        // Add immediately like CobaltActivity does
        int index = mAsPanel ? mParentView.getChildCount() : 0;
        mParentView.addView(mSurfaceView, index, mLayoutParams);

        mCallbacks = new Callbacks();
        mSurfaceView.getHolder().addCallback(mCallbacks);

    }

    @Override
    public void release() {
        removeSurfaceViewQuietly();
        mHost = null;
    }

    private boolean copyRectToLayoutParams(final Rect rect) {
        if (mLayoutParams.leftMargin == rect.x && mLayoutParams.topMargin == rect.y
                && mLayoutParams.width == rect.width && mLayoutParams.height == rect.height) {
            return false;
        }

        mLayoutParams.leftMargin = rect.x;
        mLayoutParams.topMargin = rect.y;
        mLayoutParams.width = rect.width;
        mLayoutParams.height = rect.height;
        return true;
    }

    @Override
    public void layoutSurface(final Rect rect) {
        if (mSurfaceView == null) return;
        if (copyRectToLayoutParams(rect)) {
            mSurfaceView.setLayoutParams(mLayoutParams);
        }
    }

    @Override
    public void onWindowToken(IBinder token) {
        if (mSurfaceView == null) return;

        if (token == null) {
            if (mHost != null) mHost.onOverlayDestroyed();
            mHost = null;
            removeSurfaceViewQuietly();
            return;
        }
        
        // We already added it in initialize().
        // Just ensure it's visible.
        if (mSurfaceView != null) {
            mSurfaceView.setVisibility(android.view.View.VISIBLE);
        }
    }

    private void removeSurfaceViewQuietly() {
        if (mSurfaceView != null && mParentView != null) {
            try {
                mParentView.removeView(mSurfaceView);
            } catch (RuntimeException e) {
                Log.w(TAG, "Failed to remove SurfaceView");
            }
        }
        mSurfaceView = null;
        mCallbacks = null;
        mParentView = null;
    }

    private class Callbacks implements SurfaceHolder.Callback2 {
        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            if (mSurfaceView == null) return;
            if (mHost != null) mHost.onSurfaceReady(holder.getSurface());
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            if (mSurfaceView == null || mHost == null) return;
            mHost.onOverlayDestroyed();
            mHost = null;
        }

        @Override
        public void surfaceRedrawNeeded(SurfaceHolder holder) {}
    }
}