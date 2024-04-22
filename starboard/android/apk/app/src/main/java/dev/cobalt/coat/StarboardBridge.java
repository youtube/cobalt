// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import dev.cobalt.util.Log;
import java.util.Arrays;
import java.util.HashMap;

/**
 * Mock Starboard bridge
 */
public class StarboardBridge {
  private static final String TAG = "CobaltService";

  private final HashMap<String, CobaltService.Factory> cobaltServiceFactories = new HashMap<>();
  private final HashMap<String, CobaltService> cobaltServices = new HashMap<>();
  /**
   * Host app interface
   */
  public interface HostApplication {
    void setStarboardBridge(StarboardBridge starboardBridge);

    StarboardBridge getStarboardBridge();
  }

  private final VolumeStateReceiver volumeStateReceiver;
  private final CobaltSystemConfigChangeReceiver sysConfigChangeReceiver;

  private final Runnable stopRequester =
      new Runnable() {
        @Override
        public void run() {
          requestStop(0);
        }
      };

  public StarboardBridge(Context appContext) {
    this.sysConfigChangeReceiver = new CobaltSystemConfigChangeReceiver(appContext, stopRequester);
    this.volumeStateReceiver = new VolumeStateReceiver(appContext);
  }

  public void requestStop(int errorLevel) {
      Log.i(TAG, "Request to stop");
  }


  public void registerCobaltService(CobaltService.Factory factory) {
    Log.i(TAG, "Registered " + factory.getServiceName());
    cobaltServiceFactories.put(factory.getServiceName(), factory);
  }

  boolean hasCobaltService(String serviceName) {
    boolean weHaveIt =cobaltServiceFactories.get(serviceName) != null;
    Log.e(TAG, "From bridge, hasCobaltService:" + serviceName + " got? : " + weHaveIt);
    return weHaveIt;
  }

  CobaltService openCobaltService(long nativeService, String serviceName) {
    if (cobaltServices.get(serviceName) != null) {
      // Attempting to re-open an already open service fails.
      Log.e(TAG, String.format("Cannot open already open service %s", serviceName));
      return null;
    }
    final CobaltService.Factory factory = cobaltServiceFactories.get(serviceName);
    if (factory == null) {
      Log.e(TAG, String.format("Cannot open unregistered service %s", serviceName));
      return null;
    }
    CobaltService service = factory.createCobaltService(nativeService);
    if (service != null) {
      //service.receiveStarboardBridge(this);
      cobaltServices.put(serviceName, service);
    }
    return service;
  }

  void closeCobaltService(String serviceName) {
    Log.i(TAG, String.format("Close service: %s", serviceName));
    cobaltServices.remove(serviceName);
  }

  // Differing impl
  void sendToCobaltService(String serviceName, byte [] data) {
    Log.i(TAG, String.format("Send to : %s data: %s", serviceName, Arrays.toString(data)));
    CobaltService service = cobaltServices.get(serviceName);
    if (service == null) {
      // Attempting to re-open an already open service fails.
      Log.e(TAG, String.format("Service not opened: %s", serviceName));
      return;
    }
    service.receiveFromClient(data);
  }


}
