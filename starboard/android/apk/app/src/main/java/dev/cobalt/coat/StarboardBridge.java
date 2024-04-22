package dev.cobalt.coat;
import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import dev.cobalt.util.Log;

public class StarboardBridge {

  public interface HostApplication {
    void setStarboardBridge(StarboardBridge starboardBridge);

    StarboardBridge getStarboardBridge();
  }

  private VolumeStateReceiver volumeStateReceiver;
  private CobaltSystemConfigChangeReceiver sysConfigChangeReceiver;

  private final Runnable stopRequester =
      new Runnable() {
        @Override
        public void run() {
          requestStop(0);
        }
      };


  public StarboardBridge(
      Context appContext) {
    this.sysConfigChangeReceiver = new CobaltSystemConfigChangeReceiver(appContext, stopRequester);
    this.volumeStateReceiver = new VolumeStateReceiver(appContext);
  }

  public void requestStop(int errorLevel) {
    Log.i(TAG, "Request to stop");
  }

}
