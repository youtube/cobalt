package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.webkit.JavascriptInterface;
import dev.cobalt.util.Log;
import dev.cobalt.util.SystemPropertiesHelper;

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
  public String getSystemProperty(String propertyName, String defaultValue) {
    return SystemPropertiesHelper.getSystemProperty(propertyName, defaultValue);
  }

  @JavascriptInterface
  public String getRestrictedSystemProperty(String propName, String defaultValue) {
    return SystemPropertiesHelper.getRestrictedSystemProperty(propName, defaultValue);
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
