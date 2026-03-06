package dev.cobalt.libraries.services.clientloginfo;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import java.text.SimpleDateFormat; // Add this import
import java.util.Date;             // Add this import
import java.util.Locale;           // Add this import


/** ClientLogInfo to report Android API support on android devices. */
public class ClientLogInfo extends CobaltService {
  public static final String TAG = "ClientLogInfo";

  protected static final String SERVICE_NAME = "dev.cobalt.coat.clientloginfo";

  private static String sClientInfo = "";
  private final long mNativeService;
  // Changed to ScheduledExecutorService
  private final ScheduledExecutorService mExecutor;

  public ClientLogInfo(Context appContext, long nativeService) {
    Log.i(TAG, "Opening ClientLogInfo");
    this.mNativeService = nativeService;

    // Use a scheduled pool instead of a fixed pool
    this.mExecutor = Executors.newSingleThreadScheduledExecutor();

    // Schedule the task to run IMMEDIATELY (0 delay) and then every 1 MINUTE
    mExecutor.scheduleAtFixedRate(
        this::sendRecurringUpdate, 
        0, 
        10, 
        TimeUnit.SECONDS
    );
  }

  /** Helper method to encapsulate the data formatting and sending logic */
  private void sendRecurringUpdate() {
    try {
      // Create a readable timestamp: HH:mm:ss
      String timeStamp = new SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(new Date());
      
      String responseString =
          "displayRefreshRate:" + DisplayUtil.getDefaultDisplayRefreshRate() + ";" + sClientInfo;
      
      // Added [TIMESTAMP] to the log message
      String asynResponseString = "[" + timeStamp + "] recurring update: " + responseString;
      
      Log.i(TAG, "Platform service send recurring responseString:" + asynResponseString);
      sendToClient(mNativeService, asynResponseString.getBytes(UTF_8));
      
    } catch (Exception e) {
      Log.e(TAG, "Error sending recurring update", e);
    }
  }

  @Override
  public void beforeStartOrResume() {}

  @Override
  public void beforeSuspend() {}

  @Override
  public void afterStopped() {}

  @Override
  public ResponseToClient receiveFromClient(byte[] data) {
    String dataString = new String(data, UTF_8);
    Log.i(TAG, "Received data from platform service client:" + dataString);

    ResponseToClient response = new ResponseToClient();
    response.invalidState = false;

    final String responseString =
        "displayRefreshRate:" + DisplayUtil.getDefaultDisplayRefreshRate() + ";" + sClientInfo;

    response.data = responseString.getBytes(UTF_8);

    // Manual triggers still work as expected via the executor
    mExecutor.execute(
      () -> {
        String asynResponseString = "async response: " + responseString;
        Log.i(TAG, "Platform service send async responseString:" + asynResponseString);
        sendToClient(mNativeService, asynResponseString.getBytes(UTF_8));
      }
    );

    Log.i(TAG, "Platform service send sync responseString:" + responseString);
    return response;
  }

  @Override
  public void close() {
    // Shuts down both the scheduled tasks and the executor
    mExecutor.shutdown();
  }

  public static void setClientInfo(String value) {
    sClientInfo = value;
  }
}