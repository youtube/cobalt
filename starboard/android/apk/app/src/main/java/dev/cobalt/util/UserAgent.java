package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.content.pm.PackageManager;


public class UserAgent {
  private final Context appContext;

  public UserAgent(Context context) {
    this.appContext = context;
  }

  public String createUserAgentString() {
    // TODO: sanitize inputs
    String brand = SystemPropertiesHelper.getRestrictedSystemProperty("ro.product.brand","defaultBrand");
    String model = SystemPropertiesHelper.getRestrictedSystemProperty("ro.product.model","defaultModel");
    String firmware = SystemPropertiesHelper.getRestrictedSystemProperty("ro.build.id","defaultFirmware");
    String chipset = SystemPropertiesHelper.getRestrictedSystemProperty("ro.board.platform","defaultChipset");
    String oemKey = SystemPropertiesHelper.getRestrictedSystemProperty("ro.oem.key1","defaultModelYear");
    String integrator = SystemPropertiesHelper.getRestrictedSystemProperty("ro.product.manufacturer","defaultIntegrator");
    String androidVersion = SystemPropertiesHelper.getRestrictedSystemProperty("ro.build.version.release","defaultAndroidVersion");
    String abi = SystemPropertiesHelper.getRestrictedSystemProperty("ro.product.cpu.abi", "defaultABI");
    String aux = this.getUserAgentAuxField();
    String modelYear = "20" + oemKey.substring(9, 11);

    // TODO: Resolve missing and hardcoded fields
    String customUserAgent = String.format("Mozilla/5.0 (Linux %s; Android %s) %s (unlike Gecko)" +
            " v8/8.8.278.8-jit gles Starboard/%s, %s_ATV_%s_%s/%s" +
            " (%s, %s) %s",
        abi, androidVersion,
        "Cobalt/26.lts.99.42-gold","17",
        integrator, chipset, modelYear, firmware,
        brand, model, aux
    );
    Log.e(TAG, "Custom User-Agent: " + customUserAgent);
    return customUserAgent;
  }

  protected String getUserAgentAuxField() {
    StringBuilder sb = new StringBuilder();

    String packageName = appContext.getApplicationInfo().packageName;
    sb.append(packageName);
    sb.append('/');

    try {
      sb.append(appContext.getPackageManager().getPackageInfo(packageName, 0).versionName);
    } catch (PackageManager.NameNotFoundException ex) {
      // Should never happen
      Log.e(TAG, "Can't find our own package", ex);
    }

    return sb.toString();
  }


}
