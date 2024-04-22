package dev.cobalt.libraries.services;
import android.content.Context;
import dev.cobalt.coat.CobaltService;

/** Fake SoftMic Module */
public class FakeSoftMicModule {
  public CobaltService.Factory provideFactory(final Context context) {
    return new CobaltService.Factory() {
      @Override
      public CobaltService createCobaltService(long nativeService) {
        return new FakeSoftMic(context, nativeService);
      }

      @Override
      public String getServiceName() {
        return FakeSoftMic.SERVICE_NAME;
      }
    };
  }
}
