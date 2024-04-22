package dev.cobalt.coat;


import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.CompletableFuture;

/** Helper utilities */
public class Helpers {

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
