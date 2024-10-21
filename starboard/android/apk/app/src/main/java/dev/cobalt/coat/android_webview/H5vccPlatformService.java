package dev.cobalt.coat.android_webview;

import android.util.Base64;
import android.webkit.JavascriptInterface;
import dev.cobalt.coat.StarboardBridge;

public class H5vccPlatformService implements WebAppInterface {
    StarboardBridge bridge;

    // Instantiate the interface and set the context
    public H5vccPlatformService(StarboardBridge b) {
        bridge = b;
    }

    @Override
    public String getJavaScriptInterfaceName() {
        return "Android_H5vccPlatformService";
    }

    @JavascriptInterface
    public boolean has_platform_service(String servicename) {
        return bridge.hasCobaltService(servicename);
    }

    @JavascriptInterface
    public void open_platform_service(long number, String servicename) {
        bridge.openCobaltService(number, servicename);
    }

    @JavascriptInterface
    public void close_platform_service(String servicename) {
        bridge.closeCobaltService(servicename);
    }

    @JavascriptInterface
    public void platform_service_send(String servicename, String base64Data) {
        byte[] data = Base64.decode(base64Data, Base64.DEFAULT);
        bridge.sendToCobaltService(servicename, data);
    }
}
