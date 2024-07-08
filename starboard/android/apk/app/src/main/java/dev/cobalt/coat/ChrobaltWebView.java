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
import dev.cobalt.coat.BuildConfig;

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

    private String createUserAgentString() {
        // TODO: sanitize inputs
        String brand = this.webAppInterface.getRestrictedSystemProperty("ro.product.brand","defaultBrand");
        String model = this.webAppInterface.getRestrictedSystemProperty("ro.product.model","defaultModel");
        String firmware = this.webAppInterface.getRestrictedSystemProperty("ro.build.id","defaultFirmware");
        String chipset = this.webAppInterface.getRestrictedSystemProperty("ro.board.platform","defaultChipset");
        String oemKey = this.webAppInterface.getRestrictedSystemProperty("ro.oem.key1","defaultModelYear");
        String integrator = this.webAppInterface.getRestrictedSystemProperty("ro.product.manufacturer","defaultIntegrator");
        String androidVersion = this.webAppInterface.getRestrictedSystemProperty("ro.build.version.release","defaultAndroidVersion");
        String abi = this.webAppInterface.getRestrictedSystemProperty("ro.product.cpu.abi", "defaultABI");
        String aux = this.bridge.getUserAgentAuxField();
        String modelYear = "20" + oemKey.substring(9, 11);

        // TODO: Resolve missing and hardcoded fields
        String customUserAgent = String.format("Mozilla/5.0 (Linux %s; Android %s) %s (unlike Gecko)" +
            " v8/8.8.278.8-jit gles Starboard/%s, %s_ATV_%s_%s/%s" +
            " (%s, %s) %s",
            abi, androidVersion,
            "Cobalt/26.lts.99.42-gold","17",
            integrator, chipset, modelYear, firmware,
            brand, model, aux
            );
        Log.e(TAG, "Custom User-Agent: " + customUserAgent);
        return customUserAgent;
    }

    public ChrobaltWebView(@NonNull Context context, @NonNull StarboardBridge bridge) {
        super(context);

        this.bridge = bridge;
        this.webAppInterface  = new WebAppInterface(context, this.bridge);
        this.webViewClient = new ChrobaltWebViewClient();

        WebSettings webSettings = this.getSettings();

        // Enable JavaScript
        webSettings.setJavaScriptEnabled(true);

        webSettings.setUserAgentString(createUserAgentString());

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
