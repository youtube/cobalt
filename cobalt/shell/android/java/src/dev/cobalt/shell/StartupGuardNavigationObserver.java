package dev.cobalt.shell;

import android.util.Log;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

/**
 * An observer that monitors web content navigation to disarm the StartupGuard
 * once the primary page has successfully loaded or begun navigating.
 */
public class StartupGuardNavigationObserver extends WebContentsObserver {
    private static final String YOUTUBE_URL = "https://www.youtube.com/tv";

    /**
     * Constructs a new observer for the given WebContents.
     *
     * @param webContents The WebContents to observe.
     */
    public StartupGuardNavigationObserver(WebContents webContents) {
        // In M138, we explicitly call observe to attach to the WebContents.
        observe(webContents);
    }

    @Override
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        if (navigation.getUrl() != null &&
            navigation.getUrl().getSpec().startsWith(YOUTUBE_URL)) {
            Log.v("cobalt", "ColinL setStartupMilestone:33 - Main navigation to YouTube started.");
            StartupGuard.getInstance().setStartupMilestone(33);
        }
    }

    @Override
    public void didRedirectNavigation(NavigationHandle navigation) {
        if (navigation.getUrl() != null &&
            navigation.getUrl().getSpec().startsWith(YOUTUBE_URL)) {
            Log.v("cobalt", "ColinL setStartupMilestone:36 - Navigation redirected to YouTube URL.");
            StartupGuard.getInstance().setStartupMilestone(36);
        }

        // If the page starts to navigate, the app is functioning.
        // Network-specific errors (e.g., DNS failure, offline) are handled by
        // separate error-handling logic, so we disarm the guard here.
        StartupGuard.getInstance().disarm();
        observe(null);
    }

    @Override
    public void didStartLoading(GURL url) {
        if (url != null && url.getSpec().startsWith(YOUTUBE_URL)) {
            Log.v("cobalt", "ColinL setStartupMilestone:32 - YouTube URL started loading.");
            StartupGuard.getInstance().setStartupMilestone(32);
        }
    }

    @Override
    public void didFailLoad(
        boolean isInPrimaryMainFrame, int errorCode, GURL failingUrl, int rfhLifecycleState) {
        if (failingUrl != null && failingUrl.getSpec().startsWith(YOUTUBE_URL)) {
            Log.v("cobalt", "ColinL setStartupMilestone:34 - YouTube URL failed to load. Error: " + errorCode);
            StartupGuard.getInstance().setStartupMilestone(34);
        }
    }

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        if (navigation.getUrl() != null &&
            navigation.getUrl().getSpec().startsWith(YOUTUBE_URL)) {
            Log.v("cobalt", "ColinL setStartupMilestone:35 - YouTube navigation finished successfully.");
            StartupGuard.getInstance().setStartupMilestone(35);
        }

        StartupGuard.getInstance().disarm();
        observe(null);
    }
}
