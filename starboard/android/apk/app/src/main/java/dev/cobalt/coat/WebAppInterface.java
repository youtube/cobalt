package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.util.Base64;
import android.webkit.JavascriptInterface;
import dev.cobalt.util.Log;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/** Native call bridge */
public class WebAppInterface {
  Context mContext;
  StarboardBridge bridge;

  // Instantiate the interface and set the context
  WebAppInterface(Context c, StarboardBridge b) {
    mContext = c;
    bridge = b;
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

  @JavascriptInterface
  public String getSystemProperty(String propertyName, String defaultValue) {
    return System.getProperty(propertyName, defaultValue);
  }

  @JavascriptInterface
  public String getRestrictedSystemProperty(String propName, String defaultValue) {
    try {
      Process process = Runtime.getRuntime().exec("getprop " + propName);
      BufferedReader bufferedReader =
          new BufferedReader(new InputStreamReader(process.getInputStream()));

      return bufferedReader.readLine();
    } catch (IOException e) {
      return defaultValue;
    }
  }

  @JavascriptInterface
  public String getAdvertisingId() {
    Log.i(TAG, "getAdvertisingId:" + bridge.getAdvertisingId());
    return bridge.getAdvertisingId();
  }

  @JavascriptInterface
  public boolean getLimitAdTracking() {
    return bridge.getLimitAdTracking();
  }
}
