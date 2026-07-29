package dev.cobalt.util;


import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.io.IOException;

/** Utility functions for read asset. */
public class AssetLoader {

    private AssetLoader() {}

    public static String loadJavaScriptFromAssets(Context context, String filename) {
                StringBuilder sb = new StringBuilder();
        InputStream inputStream = null;
        BufferedReader reader = null;
        try {
            inputStream = context.getAssets().open(filename);
            reader = new BufferedReader(new InputStreamReader(inputStream, StandardCharsets.UTF_8));
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            return sb.toString();
        } catch (IOException ex) {
            String error = "asset " + filename + " failed to load";
            Log.e(TAG, error);
            return String.format("console.error('%s');", error);
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Error closing reader: " + e.getMessage());
            }
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException e) {
                Log.e(TAG, "Error closing input stream: " + e.getMessage());
            }
        }
    }
}
