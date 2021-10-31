package dev.cobalt.libraries.services.clientloginfo;

import android.content.Context;
import dev.cobalt.coat.CobaltService;

/** Module for the ClientLogInfo */
public class ClientLogInfoModule {

  public CobaltService.Factory provideFactory(final Context context) {
    return new CobaltService.Factory() {
      @Override
      public CobaltService createCobaltService(long nativeService) {
        return new ClientLogInfo(context, nativeService);
      }

      @Override
      public String getServiceName() {
        return ClientLogInfo.SERVICE_NAME;
      }
    };
  }
}
