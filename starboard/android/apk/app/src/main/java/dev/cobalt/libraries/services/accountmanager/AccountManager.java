package dev.cobalt.libraries.services.accountmanager;

import static dev.cobalt.util.Log.TAG;
import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.util.Log;
import java.util.concurrent.Executor;
import org.json.JSONException;
import org.json.JSONObject;

public class AccountManager extends CobaltService {
  // The application uses this identifier to open the service.
  protected static final String SERVICE_NAME = "com.google.youtube.tv.accountmanager";

  private final Executor backgroundExecutor;
  private long nativeService = 0;

  public AccountManager(Context appContext, long nativeService, Executor exec) {
    Log.i(TAG, "Creating AccountManager");
    this.backgroundExecutor = exec;
    this.nativeService = nativeService;
  }

  @Override
  public void beforeStartOrResume() {}

  @Override
  public void beforeSuspend() {}

  @Override
  public void afterStopped() {}

  @Override
  public ResponseToClient receiveFromClient(byte[] data) {
    String text = new String(data, UTF_8);
    Log.i(TAG, "AccountManager:receiveFromClient got:" + text);

    // There is no synchronous response to send, so return an empty response.
    ResponseToClient response = new ResponseToClient();
    response.data = new byte[0];
    response.invalidState = false;

    if (backgroundExecutor != null) {
      Log.e(TAG, "Sending response");
      backgroundExecutor.execute(
          () -> {
            String dataString = new String(data, UTF_8);
            ActionResponse actionResponse = handleAction(dataString);
            sendToClient(this.nativeService, convertToBytes(actionResponse));
          });
    } else {
      Log.e(TAG, "backgroundExecutor is null. AccountManager handleAction() failed to run.");
      response.invalidState = true;
    }

    return response;
  }

  public ActionResponse handleAction(String request) {
    ActionResponse actionResponse = new ActionResponse();
    actionResponse.message += "false";
    return actionResponse;
  }

  private static byte[] convertToBytes(ActionResponse actionResponse) {
    try {
      JSONObject responseJSONObject = new JSONObject();
      responseJSONObject.put(ActionResponse.SEQ_ID, actionResponse.seqId);
      responseJSONObject.put(ActionResponse.ACTION, actionResponse.action);

      responseJSONObject.put(ActionResponse.MESSAGE, actionResponse.message);
      responseJSONObject.put(ActionResponse.ERROR, actionResponse.error);
      return responseJSONObject.toString().getBytes(UTF_8);
    } catch (JSONException e) {
      Log.w(TAG, String.format("Failed to convert actionResponse: %s to JSON", actionResponse), e);
    }
    return new byte[0];
  }

  @Override
  public void onClose() {
    super.onClose();
  }

  @Override
  public void close() {}

  @Override
  protected void sendToClient(long nativeService, byte[] data) {
    Log.e(TAG, "sendToClient native:" + nativeService);
    super.sendToClient(nativeService, data);
  }
}
