package dev.cobalt.coat;

import android.content.Context;
import android.webkit.WebView;
import android.webkit.WebSettings;
import androidx.annotation.NonNull;

import java.util.List;
import java.util.ArrayList;

import dev.cobalt.coat.android_webview.H5vccPlatformService;
import dev.cobalt.coat.android_webview.JavaScriptAndroidObject;
import dev.cobalt.util.UserAgent;

public class ChrobaltWebView extends WebView {
    StarboardBridge bridge = null;

    WebAppInterface webAppInterface = null;

    ChrobaltWebViewClient webViewClient = null;

    public ChrobaltWebView(@NonNull Context context, @NonNull StarboardBridge bridge) {
        super(context);

        this.bridge = bridge;

        // Todo, kill this
        this.webAppInterface  = new WebAppInterface(context, this.bridge);

        List<JavaScriptAndroidObject> javaScriptAndroidObjectList = new ArrayList<>();
        javaScriptAndroidObjectList.add(new H5vccPlatformService(bridge));

        this.webViewClient = new ChrobaltWebViewClient(javaScriptAndroidObjectList);

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
        for (JavaScriptAndroidObject javascriptAndroidObject : javaScriptAndroidObjectList) {
            // Have to cast to the class to avoid
            // Error: None of the methods in the added interface (JavaScriptAndroidObject) have been annotated with @android.webkit.JavascriptInterface; they will not be visible in API 17 [JavascriptInterface]
            if (javascriptAndroidObject instanceof H5vccPlatformService){
                addJavascriptInterface((H5vccPlatformService)javascriptAndroidObject, javascriptAndroidObject.getJavaScriptInterfaceName());
            }
//            addJavascriptInterface(javascriptAndroidObject, javascriptAndroidObject.getJavaScriptInterfaceName());
        }

        addJavascriptInterface(this.webAppInterface, "Android");

        setWebViewClient(this.webViewClient);
    }

    public void evalJavaScript(String javascript) { // Make sure it's public
        this.evaluateJavascript(javascript, null);
    }
}
