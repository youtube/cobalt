package org.chromium.content_shell_apk.chrobalt;

import org.chromium.content_shell.Shell;
import android.util.Log;

public class ChrobaltJavaScriptAndroidObject {
    
    private final Shell shell;
    public ChrobaltJavaScriptAndroidObject(Shell shell) {
        this.shell = shell;
    }

    @ChrobaltJavaScriptInterface
    public void testJavaScriptMethod() {
        Log.w("Colin", "testJavaScriptMethod()");
        this.shell.loadUrl("https://www.youtube.com/tv");
    }
}
