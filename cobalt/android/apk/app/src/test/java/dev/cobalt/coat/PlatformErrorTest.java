package dev.cobalt.coat;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Dialog;
import dev.cobalt.shell.Shell;
import dev.cobalt.util.Holder;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for PlatformError. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlatformErrorTest {

  private static final String TEST_URL = "https://www.youtube.com/tv";
  private static final int TEST_DATA = 0;
  private static final int RETRY_BUTTON_ID = 1;
  private static final int DISMISS_BUTTON_ID = 3;

  private PlatformError platformError;

  @Before
  public void setUp() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    Holder<Activity> holder = new Holder<>();
    holder.set(mockActivity);
    platformError = new PlatformError(holder, PlatformError.CONNECTION_ERROR, TEST_DATA, "");

    PlatformError.resetRetryCount();
  }

  @Test
  public void addRetryUrlParam_appendsParam_whenNotPresent() {
    String url = TEST_URL;
    String result = platformError.addRetryUrlParam(url, 1);
    assertEquals(TEST_URL + "?netdialog_retry=1", result);
  }

  @Test
  public void addRetryUrlParam_updatesParam_whenPresent() {
    String url = TEST_URL + "?netdialog_retry=1";
    String result = platformError.addRetryUrlParam(url, 2);
    assertEquals(TEST_URL + "?netdialog_retry=2", result);
  }

  @Test
  public void addRetryUrlParam_preservesOtherParams() {
    String url = "https://www.youtube.com/tv?foo=bar";
    String result = platformError.addRetryUrlParam(url, 1);
    assertEquals("https://www.youtube.com/tv?foo=bar&netdialog_retry=1", result);
  }

  @Test
  public void addRetryUrlParam_preservesFragment() {
    String url = "https://www.youtube.com/tv#/browse";
    String result = platformError.addRetryUrlParam(url, 1);
    assertEquals("https://www.youtube.com/tv?netdialog_retry=1#/browse", result);
  }

  @Test
  public void addRetryUrlParam_preservesParamsAndFragment() {
    String url = "https://www.youtube.com/tv?foo=bar#/browse";
    String result = platformError.addRetryUrlParam(url, 1);
    assertEquals("https://www.youtube.com/tv?foo=bar&netdialog_retry=1#/browse", result);
  }

  @Test
  public void addRetryUrlParam_handlesEmptyUrl() {
    String url = "";
    String result = platformError.addRetryUrlParam(url, 1);
    assertEquals("?netdialog_retry=1", result);
  }

  @Test
  public void setResponse_updatesResponse() {
    platformError.setResponse(PlatformError.POSITIVE);
    try {
      int response = platformError.getResponse();
      assertEquals(PlatformError.POSITIVE, response);
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  @Test
  public void onClick_dismissesDialog_whenDismissButtonClicked() throws Exception {
    Dialog mockDialog = mock(Dialog.class);
    platformError.setDialog(mockDialog);

    platformError.onClick(null, DISMISS_BUTTON_ID);

    int response = platformError.getResponse();
    assertEquals(PlatformError.NEGATIVE, response);
    verify(mockDialog).dismiss();
  }

  @Test
  public void onClick_reloadsUrl_whenRetryButtonClickedAndUrlNotEmpty() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    Shell mockShell = mock(Shell.class);
    Dialog mockDialog = mock(Dialog.class);

    Holder<Activity> holder = new Holder<>();
    holder.set(mockActivity);

    PlatformError testPlatformError =
        new PlatformError(holder, PlatformError.CONNECTION_ERROR, 0, "https://www.youtube.com/tv");
    testPlatformError.setDialog(mockDialog);

    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockActivity.getActiveShell()).thenReturn(mockShell);

    testPlatformError.onClick(null, RETRY_BUTTON_ID);

    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=1");
    verify(mockDialog).dismiss();
  }

  @Test
  public void onClick_retryUrlParamIncrements_whenCalledMultipleTimes() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    Shell mockShell = mock(Shell.class);
    Dialog mockDialog = mock(Dialog.class);

    Holder<Activity> holder = new Holder<>();
    holder.set(mockActivity);

    PlatformError testPlatformError =
        new PlatformError(holder, PlatformError.CONNECTION_ERROR, 0, "https://www.youtube.com/tv");
    testPlatformError.setDialog(mockDialog);

    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockActivity.getActiveShell()).thenReturn(mockShell);

    // First retry
    testPlatformError.onClick(null, RETRY_BUTTON_ID);
    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=1");

    // Second retry
    testPlatformError.onClick(null, RETRY_BUTTON_ID);
    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=2");
  }

  @Test
  public void onClick_reloadsWebContents_whenRetryButtonClickedAndUrlIsEmpty() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    NavigationController mockNavController = mock(NavigationController.class);
    Dialog mockDialog = mock(Dialog.class);

    Holder<Activity> holder = new Holder<>();
    holder.set(mockActivity);

    PlatformError testPlatformError =
        new PlatformError(holder, PlatformError.CONNECTION_ERROR, 0, "");
    testPlatformError.setDialog(mockDialog);

    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockWebContents.getNavigationController())
        .thenReturn(mockNavController);
    org.chromium.url.GURL mockGurl = mock(org.chromium.url.GURL.class);
    org.mockito.Mockito.when(mockGurl.getSpec()).thenReturn("");
    org.mockito.Mockito.when(mockWebContents.getVisibleUrl()).thenReturn(mockGurl);

    testPlatformError.onClick(null, RETRY_BUTTON_ID);

    verify(mockNavController).reload(true);
    verify(mockDialog).dismiss();
  }

  @Test
  public void retry_reloadsUrlAndDismissesDialog() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    Shell mockShell = mock(Shell.class);
    Dialog mockDialog = mock(Dialog.class);

    Holder<Activity> holder = new Holder<>();
    holder.set(mockActivity);

    PlatformError testPlatformError =
        new PlatformError(holder, PlatformError.CONNECTION_ERROR, 0, "https://www.youtube.com/tv");
    testPlatformError.setDialog(mockDialog);
    org.mockito.Mockito.when(mockDialog.isShowing()).thenReturn(true);

    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockActivity.getActiveShell()).thenReturn(mockShell);

    testPlatformError.retry();

    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=1");
    verify(mockDialog).dismiss();
  }

  @Test
  public void reloadUrl_usesTargetUrl_overVisibleUrl() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    GURL mockGurl = mock(GURL.class);
    Shell mockShell = mock(Shell.class);

    org.mockito.Mockito.when(mockGurl.getSpec()).thenReturn("about:blank");
    org.mockito.Mockito.when(mockWebContents.getVisibleUrl()).thenReturn(mockGurl);
    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockActivity.getActiveShell()).thenReturn(mockShell);

    PlatformError.reloadUrl(mockActivity, mockWebContents, "https://www.youtube.com/tv");

    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=1");
  }

  @Test
  public void reloadUrl_usesVisibleUrl_whenTargetUrlIsEmpty() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    GURL mockGurl = mock(GURL.class);
    Shell mockShell = mock(Shell.class);

    org.mockito.Mockito.when(mockGurl.getSpec()).thenReturn("https://www.youtube.com/tv");
    org.mockito.Mockito.when(mockWebContents.getVisibleUrl()).thenReturn(mockGurl);
    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockActivity.getActiveShell()).thenReturn(mockShell);

    PlatformError.reloadUrl(mockActivity, mockWebContents, "");

    verify(mockShell).loadUrl("https://www.youtube.com/tv?netdialog_retry=1");
  }

  @Test
  public void reloadUrl_reloadsNavigationController_whenUrlsAreEmpty() throws Exception {
    CobaltActivity mockActivity = mock(CobaltActivity.class);
    WebContents mockWebContents = mock(WebContents.class);
    NavigationController mockNavController = mock(NavigationController.class);

    org.mockito.Mockito.when(mockActivity.getActiveWebContents()).thenReturn(mockWebContents);
    org.mockito.Mockito.when(mockWebContents.getNavigationController())
        .thenReturn(mockNavController);

    PlatformError.reloadUrl(mockActivity, mockWebContents, "");

    verify(mockNavController).reload(true);
  }
}
