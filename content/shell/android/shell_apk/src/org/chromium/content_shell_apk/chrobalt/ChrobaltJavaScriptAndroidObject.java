package org.chromium.content_shell_apk.chrobalt;

import android.util.Log;

public class ChrobaltJavaScriptAndroidObject {
    
    @ChrobaltJavaScriptInterface
    public void testJavaScriptMethod() {
        Log.w("Colin", "testJavaScriptMethod()");
    }
}
