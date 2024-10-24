package org.chromium.content_shell_apk.chrobalt;

import org.chromium.content_shell.Shell;
import android.util.Log;

public class ChrobaltJavaScriptAndroidObject {
    
    public ChrobaltJavaScriptAndroidObject() {
    }

    @ChrobaltJavaScriptInterface
    public void testJavaScriptMethod() {
        Log.w("Colin", "testJavaScriptMethod()");
    }
}
