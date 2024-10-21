package dev.cobalt.coat.android_webview;

public interface JavaScriptAndroidObject {

    // the name is used by WebView.addJavascriptInterface (Object object, String name)
    public String getJavaScriptInterfaceName();

    // the js file name under assets
    public String getJavaScriptAssets();
}
