package dev.cobalt.shell;

import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * An observer that monitors web content navigation to disarm the StartupGuard
 * once the primary page has successfully loaded or begun navigating.
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
    public void didRedirectNavigation(NavigationHandle navigation) {
        // If the page starts to navigate, the app is functioning.
        // Network-specific errors (e.g., DNS failure, offline) are handled by
        // separate error-handling logic, so we disarm the guard here.
        StartupGuard.getInstance().disarm();
        destroy();
    }

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        StartupGuard.getInstance().disarm();
        destroy();
    }
}
