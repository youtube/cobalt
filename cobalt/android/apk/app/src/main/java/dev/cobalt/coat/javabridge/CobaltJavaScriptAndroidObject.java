package dev.cobalt.coat.javabridge;

import androidx.annotation.Nullable;

/**
 * Interface for Android objects that are exposed to JavaScript.
 */
public interface CobaltJavaScriptAndroidObject {

    /**
     * Gets the name used to expose this object to JavaScript.
     * This name is used in the `addJavascriptInterface` method of the WebView.
     *
     * @return The JavaScript interface name.
     */
    public String getJavaScriptInterfaceName();

    /**
     * Gets the name of the JavaScript asset file that uses this interface.
     * This allows the JavaScript code to be loaded and interact with this object.
     *
     * @return The name of the JavaScript asset file, or null if not applicable.
     */
    public @Nullable String getJavaScriptAssetName();
}
