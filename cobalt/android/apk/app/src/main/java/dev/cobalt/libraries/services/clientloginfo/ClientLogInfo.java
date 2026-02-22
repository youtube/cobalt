package dev.cobalt.libraries.services.clientloginfo;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadPoolExecutor;


/** ClientLogInfo to report Android API support on android devices. */
public class ClientLogInfo extends CobaltService {
  public static final String TAG = "ClientLogInfo";

  // The application uses this identifier to open the service.
  protected static final String SERVICE_NAME = "dev.cobalt.coat.clientloginfo";

  private static String sClientInfo = "";
  private final long mNativeService;
  private final ThreadPoolExecutor mExecutor;

  public ClientLogInfo(Context appContext, long nativeService) {
    Log.i(TAG, "Opening ClientLogInfo");
    this.mNativeService = nativeService;

    // Create a ThreadPoolExecutor with a fixed number of threads
    this.mExecutor = (ThreadPoolExecutor) Executors.newFixedThreadPool(1);
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

    // synchronize response
    response.data = responseString.getBytes(UTF_8);

    // Submit a Runnable task to send async response
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
  public void close() {}

  public static void setClientInfo(String value) {
    sClientInfo = value;
  }
}
