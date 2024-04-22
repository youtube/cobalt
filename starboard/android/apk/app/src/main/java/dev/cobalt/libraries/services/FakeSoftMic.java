package dev.cobalt.libraries.services;


import android.content.Context;
import android.util.Log;
import dev.cobalt.coat.CobaltService;
import static java.nio.charset.StandardCharsets.UTF_8;

/**  Fake SoftMic  */
public class FakeSoftMic extends CobaltService {
  public static final String TAG = "ClientLogInfo";
  protected static final String SERVICE_NAME = "com.google.youtube.tv.SoftMic";

  public FakeSoftMic(Context appContext, long nativeService) {
    Log.i(TAG, "Opening FakeSoftMic");
  }

  @Override
  public void beforeStartOrResume() {

  }

  @Override
  public void beforeSuspend() {

  }

  @Override
  public void afterStopped() {

  }

  @Override
  public ResponseToClient receiveFromClient(byte[] data) {
    Log.i(TAG, "FakeSoftMic:receiveFromClient");
    String text = new String(data, UTF_8);
    Log.i(TAG,  "Got:" + text);
    return null;
  }

  @Override
  public void close() {

  }
}
