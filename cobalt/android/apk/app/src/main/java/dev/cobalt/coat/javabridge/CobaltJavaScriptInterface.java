package dev.cobalt.coat.javabridge;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation that allows exposing methods to JavaScript. Starting from API level
 * {@link android.os.Build.VERSION_CODES#JELLY_BEAN_MR1} and above, methods explicitly
 * marked with this annotation are available to the Javascript code.
 */
@SuppressWarnings("javadoc")
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD})
public @interface CobaltJavaScriptInterface {
}
