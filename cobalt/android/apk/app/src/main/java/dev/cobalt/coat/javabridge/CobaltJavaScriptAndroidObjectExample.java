package dev.cobalt.coat.javabridge;

import static dev.cobalt.util.Log.TAG;

import android.util.Log;

/**
 * A simple example of implement CobaltJavaScriptAndroidObject.
 */
public class CobaltJavaScriptAndroidObjectExample implements CobaltJavaScriptAndroidObject {

    @Override
    public String getJavaScriptInterfaceName() {
        return "AndroidExample";
    }

    @Override
    public String getJavaScriptAssetName() {
        return "example.js";
    }

    @CobaltJavaScriptInterface
    public void testJavaScriptMethod() {
        Log.w(TAG, "Hello world");
    }
}
