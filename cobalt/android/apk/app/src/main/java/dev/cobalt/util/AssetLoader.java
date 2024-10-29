package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.io.IOException;

/** Utility functions for read asset. */
public class AssetLoader {

    private AssetLoader() {}

    public static String loadJavaScriptFromAssets(Context context, String filename) {
        try {
            InputStream is = context.getAssets().open(filename);
            int size = is.available();
            byte[] buffer = new byte[size];
            is.read(buffer);
            is.close();
            return new String(buffer, StandardCharsets.UTF_8);
        } catch (IOException ex) {
            String error = "asset " + filename + " failed to load";
            Log.e(TAG, error);
            return String.format("console.error('%s');", error);
        }
    }
}
