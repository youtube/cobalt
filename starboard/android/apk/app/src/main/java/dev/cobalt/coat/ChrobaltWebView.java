package dev.cobalt.coat;
import static dev.cobalt.util.Log.TAG;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.webkit.WebView;
import android.webkit.WebSettings;
import android.webkit.WebViewClient;

import androidx.annotation.NonNull;

import dev.cobalt.coat.android_webview.H5vccPlatformService;
import dev.cobalt.util.Log;
import dev.cobalt.util.UserAgent;

public class ChrobaltWebView extends WebView {
  public void evalJavaScript(String javascript) { // Make sure it's public
    this.evaluateJavascript(javascript, null);
  }

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
            Helpers.loadJavaScriptFromURL("http://" + Helpers.getDevelopmentHostSetting(view.getContext()) +  ":8000/dyn_script.js")
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

        webSettings.setUserAgentString(new UserAgent(context).createUserAgentString());

        // Set mixed content mode to allow all content to be loaded, regardless of the security origin
        webSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);

        // Set cache mode to allow the WebView to use the default cache behavior
        webSettings.setCacheMode(WebSettings.LOAD_DEFAULT);

        // Enable DOM storage
        webSettings.setDomStorageEnabled(true);

        // Disable transition icon
        webSettings.setMediaPlaybackRequiresUserGesture(false);

        // Add all implementations of dev.cobalt.coat.android_webview.WebAppInterface
        H5vccPlatformService h5vccPlatformService = new H5vccPlatformService(bridge);
        addJavascriptInterface(h5vccPlatformService, h5vccPlatformService.getJavaScriptInterfaceName());

        addJavascriptInterface(this.webAppInterface, "Android");

        setWebViewClient(this.webViewClient);
    }
}
