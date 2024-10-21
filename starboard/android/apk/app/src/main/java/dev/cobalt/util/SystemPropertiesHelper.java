package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Method;

public class SystemPropertiesHelper {
    private static Method getStringMethod;

    static {
        try {
            getStringMethod =
                    ClassLoader.getSystemClassLoader()
                            .loadClass("android.os.SystemProperties")
                            .getMethod("get", String.class);
            if (getStringMethod == null) {
                Log.e(TAG, "Couldn't load system properties getString");
            }
        } catch (Exception exception) {
            Log.e(TAG, "Exception looking up system properties methods: ", exception);
        }
    }

    private SystemPropertiesHelper() {}

    public static String getString(String property) {
        if (getStringMethod != null) {
            try {
                return (String) getStringMethod.invoke(null, new Object[] {property});
            } catch (Exception exception) {
                Log.e(TAG, "Exception getting system property: ", exception);
            }
        }
        return null;
    }

    public static String getRestrictedSystemProperty(String propName, String defaultValue) {
        try {
            Process process = Runtime.getRuntime().exec("getprop " + propName);
            BufferedReader bufferedReader =
                    new BufferedReader(new InputStreamReader(process.getInputStream()));

            return bufferedReader.readLine();
        } catch (IOException e) {
            return defaultValue;
        }
    }

    public static String getSystemProperty(String propertyName, String defaultValue) {
        return System.getProperty(propertyName, defaultValue);
    }
}
