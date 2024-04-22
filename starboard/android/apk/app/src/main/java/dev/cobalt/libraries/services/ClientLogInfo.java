package dev.cobalt.libraries.services;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.util.Log;

public class ClientLogInfo extends CobaltService {
  public static final String TAG = "ClientLogInfo";

  // The application uses this identifier to open the service.
  protected static final String SERVICE_NAME = "dev.cobalt.coat.clientloginfo";

  public ClientLogInfo(Context appContext, long nativeService) {
    Log.i(TAG, "Opening ClientLogInfo");
  }

}
