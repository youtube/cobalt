package dev.cobalt.coat;

/**
 * Provide a Null factory as some platform service might need it for legacy
 * apk versions.
 */
public class NullCobaltFactory implements CobaltService.Factory {

  @Override
  public CobaltService createCobaltService(long nativeService) {
    return null;
  }

  @Override
  public String getServiceName() {
    return null;
  }
}
