package dev.cobalt.shell;

import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * An observer that monitors web content navigation to disarm the StartupGuard
 * once the primary page has successfully loaded.
 */
public class StartupGuardNavigationObserver extends WebContentsObserver {
    /**
     * Constructs a new observer for the given WebContents.
     *
     * @param webContents The WebContents to observe.
     */
    public StartupGuardNavigationObserver(WebContents webContents) {
        super(webContents);
    }

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        if (navigation.hasCommitted()) {
            StartupGuard.getInstance().disarm();
            destroy();
        }
    }
}
