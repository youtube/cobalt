package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;


import android.content.Context;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.CompletableFuture;
import java.nio.charset.StandardCharsets;
import java.io.IOException;
import dev.cobalt.util.Log;

import android.content.SharedPreferences;


/** Helper utilities */
public class Helpers {
    private static final String APP_SETTINGS = "AppSettings";

    public static void setAppSetting(Context context, String key, String value) {
        SharedPreferences prefs = context.getSharedPreferences(APP_SETTINGS, Context.MODE_PRIVATE);
        prefs.edit().putString(key, value).apply();
    }
    public static String getDevelopmentHostSetting(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(APP_SETTINGS, Context.MODE_PRIVATE);
        return prefs.getString("development_host", BuildConfig.DEFAULT_DEVELOPMENT_HOST);
    }

    public static String loadJavaScriptFromAsset(Context context, String filename) {
        try {
          InputStream is = context.getAssets().open(filename);
          int size = is.available();
          byte[] buffer = new byte[size];
          is.read(buffer);
          is.close();
          return new String(buffer, StandardCharsets.UTF_8);
        } catch (IOException ex) {
          Log.e(TAG, "asset " + filename + " failed to load");
          return String.format("console.error('asset %s failed to load');", filename);
        }
      }

    // Only for development
    public static CompletableFuture<String> loadJavaScriptFromURL(final String urlString) {
        CompletableFuture<String> future = new CompletableFuture<>();

        new Thread(() -> {
            HttpURLConnection urlConnection = null;
            try {
                URL url = new URL(urlString);
                urlConnection = (HttpURLConnection) url.openConnection();
                InputStream in = new BufferedInputStream(
                        urlConnection.getInputStream());
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buffer = new byte[1024];
                int n;
                while ((n = in.read(buffer)) != -1) {
                    out.write(buffer, 0, n);
                }
                String jsCode = out.toString("UTF-8");
                 // Successfully complete the future with the JS code
                future.complete(jsCode);
            } catch (Exception e) {
                 // Complete the future exceptionally in case of failure
                future.completeExceptionally(e);
            } finally {
                if (urlConnection != null) {
                    urlConnection.disconnect();
                }
            }
        }).start();

        return future;
    }
}
