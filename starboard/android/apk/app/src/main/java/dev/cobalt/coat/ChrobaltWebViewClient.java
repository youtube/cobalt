package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.app.Activity;
import android.graphics.Bitmap;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.util.List;

import dev.cobalt.coat.android_webview.JavaScriptAndroidObject;
import dev.cobalt.util.Log;

public class ChrobaltWebViewClient extends WebViewClient {
    private final List<JavaScriptAndroidObject> javaScriptAndroidObjectList;

    public ChrobaltWebViewClient(List<JavaScriptAndroidObject> javaScriptAndroidObjectList) {
        super();
        this.javaScriptAndroidObjectList = javaScriptAndroidObjectList;
    }

    @Override
    public void onPageStarted(WebView view, String url, Bitmap favicon) {
        Log.i(TAG, "Page started loading: " + url);
        super.onPageStarted(view, url, favicon);

        for (JavaScriptAndroidObject javaScriptAndroidObject : javaScriptAndroidObjectList) {
            String jsCode = Helpers.loadJavaScriptFromAsset(view.getContext(), javaScriptAndroidObject.getJavaScriptAssets());
            view.evaluateJavascript(jsCode, null);
        }

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
