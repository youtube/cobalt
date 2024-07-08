package dev.cobalt.coat;
import static dev.cobalt.util.Log.TAG;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.webkit.WebView;
import android.webkit.WebSettings;
import android.webkit.WebViewClient;

import androidx.annotation.NonNull;

import dev.cobalt.util.Log;

public class ChrobaltWebView extends WebView {
    StarboardBridge bridge = null;

    WebAppInterface webAppInterface = null;

    ChrobaltWebViewClient webViewClient = null;

    private class ChrobaltWebViewClient extends WebViewClient {
        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            Log.i(TAG, "Page started loading: " + url);
            super.onPageStarted(view, url, favicon);

            // Load from a bundled asset
            String jsCode = Helpers.loadJavaScriptFromAsset(view.getContext(), "injected_script.js");
            view.evaluateJavascript(jsCode, null);

            // Load over network from development host
            Helpers.loadJavaScriptFromURL("http://192.168.5.188:8000/dyn_script.js")
                    .thenAccept(jsCode2 -> {

                        if ( view.getContext() instanceof Activity) {
                            ((Activity) view.getContext()).runOnUiThread(() -> {
                                // Perform UI operations here
                                Log.i(TAG, "Got JS2, injecting");
                                view.evaluateJavascript(jsCode2, null);
                            });
                        }

                    }).exceptionally(e -> {
                        // Handle any exceptions here
                        Log.e(TAG, "Error message: " + e.getMessage(), e);
                        return null;
                    });

            Log.i(TAG, "JavaScript injected");
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            Log.i(TAG, "Page finished loading: " + url);
            super.onPageFinished(view, url);
        }

    }

    public ChrobaltWebView(@NonNull Context context, @NonNull StarboardBridge bridge) {
        super(context);

        this.bridge = bridge;
        this.webAppInterface  = new WebAppInterface(context, this.bridge);
        this.webViewClient = new ChrobaltWebViewClient();

        WebSettings webSettings = this.getSettings();

        // Enable JavaScript
        webSettings.setJavaScriptEnabled(true);

        // Set a custom user-agent, TODO: provide real implementation
        String customUserAgent = "Mozilla/5.0 (Linux armeabi-v7a; Android 12) "
                + "Cobalt/26.lts.99.42-gold (unlike Gecko) v8/8.8.278.8-jit gles "
                + "Starboard/15, Google_ATV_sabrina_2020/STTE.231215.005 "
                + "(google, Chromecast) com.google.android.youtube.tv/6.30.300";
        webSettings.setUserAgentString(customUserAgent);

        // Set mixed content mode to allow all content to be loaded, regardless of the security origin
        webSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);

        // Set cache mode to allow the WebView to use the default cache behavior
        webSettings.setCacheMode(WebSettings.LOAD_DEFAULT);

        // Enable DOM storage
        webSettings.setDomStorageEnabled(true);

        // Disable transition icon
        webSettings.setMediaPlaybackRequiresUserGesture(false);

        addJavascriptInterface(this.webAppInterface, "Android");

        setWebViewClient(this.webViewClient);
    }
}
