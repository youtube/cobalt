package dev.cobalt.libraries.services.accountmanager;

import android.content.Context;
import dev.cobalt.coat.CobaltService;
import java.util.concurrent.Executor;

public class AccountManagerModule {

  public CobaltService.Factory provideFactory(final Context context, Executor backgroundExecutor) {
    return new CobaltService.Factory() {

      @Override
      public CobaltService createCobaltService(long nativeService) {
        return new AccountManager(context, nativeService, backgroundExecutor);
      }

      @Override
      public String getServiceName() {
        return AccountManager.SERVICE_NAME;
      }
    };
  }
}
