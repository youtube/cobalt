// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test.instrumentation_test_apk;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;

import java.util.List;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends AppCompatActivity {
    private ListenableFuture<WebSandbox> mWebSandboxFuture;
    private ListenableFuture<String> mWebSandboxVersionFuture;
    private LifeCycleListener mLifeCycleListener;

    /**
     * Use this to listen for life cycle events in tests.
     */
    public interface LifeCycleListener {
        void onNewIntent(Intent intent);
    }

    public void setLifeCycleListener(LifeCycleListener lifeCycleListener) {
        mLifeCycleListener = lifeCycleListener;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mWebSandboxFuture = WebSandbox.create(getApplicationContext());
    }

    public ListenableFuture<WebSandbox> getWebSandboxFuture() {
        return mWebSandboxFuture;
    }

    public void attachFragment(WebFragment fragment) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment)
                .commitNow();
    }

    public View getFragmentContainerView() {
        return findViewById(R.id.fragment_container_view);
    }

    public void detachFragment(WebFragment fragment) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction().setReorderingAllowed(true).remove(fragment).commitNow();
    }

    public WebFragment getAttachedFragment() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        List<Fragment> fragments = fragmentManager.getFragments();

        if (fragments.size() != 1) {
            throw new IllegalStateException("Expected to have exactly 1 WebFragment.");
        }

        return (WebFragment) fragments.get(0);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (mLifeCycleListener != null) {
            mLifeCycleListener.onNewIntent(intent);
        }
        super.onNewIntent(intent);
    }
}
