package dev.cobalt.libraries.services.clientloginfo;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.util.Log;

/**
 * ClientLogInfo to report Android API support on android devices, read go/coat-api-support for more
 * info.
 */
public class ClientLogInfo extends CobaltService {
  public static final String TAG = "ClientLogInfo";

  // The application uses this identifier to open the service.
  protected static final String SERVICE_NAME = "dev.cobalt.coat.clientloginfo";

  private static String clientInfo = "";

  public ClientLogInfo(Context appContext, long nativeService) {
    Log.i(TAG, "Opening ClientLogInfo");
  }

  @Override
  public void beforeStartOrResume() {}

  @Override
  public void beforeSuspend() {}

  @Override
  public void afterStopped() {}

  @Override
  public ResponseToClient receiveFromClient(byte[] data) {
    ResponseToClient response = new ResponseToClient();
    response.invalidState = false;
    response.data = clientInfo.getBytes(UTF_8);
    return response;
  }

  @Override
  public void close() {}

  public static void setClientInfo(String value) {
    clientInfo = value;
  }
}
