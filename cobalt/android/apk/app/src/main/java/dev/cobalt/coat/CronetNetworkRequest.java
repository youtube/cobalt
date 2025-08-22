package dev.cobalt.coat;

import android.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;

/**
 * A simple utility class to perform a network request using CronetEngine.
 * This is designed for verification and demonstration purposes.
 */
public class CronetNetworkRequest {

    private static final String TAG = "ColinL";

    private final CronetEngine cronetEngine;
    private final Executor executor;

    /**
     * Constructor for the network request handler.
     *
     * @param cronetEngine An initialized CronetEngine instance.
     */
    public CronetNetworkRequest(CronetEngine cronetEngine) {
        this.cronetEngine = cronetEngine;
        // Create a single-threaded executor to run network callbacks.
        this.executor = Executors.newSingleThreadExecutor();
    }

    /**
     * Performs a simple GET request to the specified URL.
     * Results are logged to Logcat.
     *
     * @param urlString The URL to fetch.
     */
    public void performGetRequest(String urlString) {
        Log.i(TAG, "Starting Cronet request to: " + urlString);
        UrlRequest.Builder requestBuilder = cronetEngine.newUrlRequestBuilder(
                urlString,
                new SimpleUrlRequestCallback(), // The callback to handle responses
                executor);

        // Build and start the request.
        UrlRequest request = requestBuilder.build();
        request.start();
    }

    /**
     * A simple implementation of UrlRequest.Callback that logs results and buffers the response.
     */
    private static class SimpleUrlRequestCallback extends UrlRequest.Callback {
        private final ByteArrayOutputStream bytesReceived = new ByteArrayOutputStream();
        private final WritableByteChannel receiveChannel = Channels.newChannel(bytesReceived);

        @Override
        public void onRedirectReceived(UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
            Log.i(TAG, "onRedirectReceived called. Following redirect to: " + newLocationUrl);
            // Cronet automatically follows redirects.
            request.followRedirect();
        }

        @Override
        public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
            int httpStatusCode = info.getHttpStatusCode();
            Log.i(TAG, "onResponseStarted called. HTTP status code: " + httpStatusCode);
            if (httpStatusCode >= 200 && httpStatusCode < 300) {
                // The request was successful, start reading the response body.
                // Allocate a 16KB buffer.
                request.read(ByteBuffer.allocateDirect(16 * 1024));
            }
        }

        @Override
        public void onReadCompleted(UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
            // The buffer is now filled with data, flip it to prepare for reading.
            byteBuffer.flip();
            try {
                // Write the data into our ByteArrayOutputStream.
                receiveChannel.write(byteBuffer);
            } catch (IOException e) {
                Log.e(TAG, "IOException while writing response data", e);
            }
            // Clear the buffer to prepare it for the next read.
            byteBuffer.clear();
            // Continue reading the response body.
            request.read(byteBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            Log.i(TAG, "onSucceeded called.");
            String responseBody = bytesReceived.toString();
            Log.i(TAG, "====== Cronet Response Start ======");
            Log.i(TAG, "URL: " + info.getUrl());
            Log.i(TAG, "Status: " + info.getHttpStatusCode());
            // Log only the first 500 characters to avoid flooding Logcat.
            Log.i(TAG, "Response Body: "
                + (responseBody.length() > 500 ? responseBody.substring(0, 500) : responseBody));
            Log.i(TAG, "======= Cronet Response End =======");
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
            Log.e(TAG, "onFailed called. Error: ", error);
        }
    }
}