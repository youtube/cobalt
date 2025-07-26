// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FocusedTabData, GlicBrowserHost, GlicWebClient, Observable, OpenPanelInfo, PanelState, TabData} from '/glic/glic_api/glic_api.js';
import {WebClientMode} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

interface PageElementTypes {
  status: HTMLElement;
  pageHeader: HTMLElement;
  focusedFavicon: HTMLImageElement;
  focusedUrl: HTMLInputElement;
  contextAccessIndicator: HTMLInputElement;
  focusedTabLogs: HTMLSpanElement;
  focusedFaviconV2: HTMLImageElement;
  focusedUrlV2: HTMLInputElement;
  contextAccessIndicatorV2: HTMLInputElement;
  focusedTabLogsV2: HTMLSpanElement;
  syncCookiesBn: HTMLButtonElement;
  testLogsBn: HTMLButtonElement;
  syncCookieStatus: HTMLSpanElement;
  getUserProfileInfoBn: HTMLButtonElement;
  getUserProfileInfoStatus: HTMLSpanElement;
  getUserProfileInfoImg: HTMLImageElement;
  changeProfileBn: HTMLButtonElement;
  testPermissionSwitch: HTMLButtonElement;
  microphoneSwitch: HTMLInputElement;
  geolocationSwitch: HTMLInputElement;
  tabContextSwitch: HTMLInputElement;
  newtabbn: HTMLButtonElement;
  reloadpage: HTMLButtonElement;
  getpagecontext: HTMLButtonElement;
  getPageContextStatus: HTMLSpanElement;
  URL: HTMLInputElement;
  innerTextCheckbox: HTMLInputElement;
  viewportScreenshotCheckbox: HTMLInputElement;
  pdfDataCheckbox: HTMLInputElement;
  annotatedPageContentCheckbox: HTMLInputElement;
  screenshotImg: HTMLImageElement;
  faviconImg: HTMLImageElement;
  getlocation: HTMLButtonElement;
  location: HTMLElement;
  permissionSelect: HTMLSelectElement;
  enabledSelect: HTMLSelectElement;
  closebn: HTMLButtonElement;
  attachpanelbn: HTMLButtonElement;
  detachpanelbn: HTMLButtonElement;
  refreshbn: HTMLButtonElement;
  navigateWebviewUrl: HTMLInputElement;
  audioCapStop: HTMLButtonElement;
  audioCapStart: HTMLButtonElement;
  audioStatus: HTMLElement;
  mic: HTMLAudioElement;
  audioDuckingOn: HTMLButtonElement;
  audioDuckingOff: HTMLButtonElement;
  desktopScreenshot: HTMLButtonElement;
  desktopScreenshotImg: HTMLImageElement;
  desktopScreenshotErrorReason: HTMLSpanElement;
  createTabInBackground: HTMLInputElement;
  canAttachCheckbox: HTMLInputElement;
  scrollToTextInput: HTMLInputElement;
  scrollToBn: HTMLButtonElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return document.getElementById(prop);
  },
});

function logMessage(message: string) {
  $.status.append(message.slice(0, 100000), document.createElement('br'));
}

class WebClient implements GlicWebClient {
  browser: GlicBrowserHost|undefined;

  async initialize(browser: GlicBrowserHost): Promise<void> {
    this.browser = browser;

    logMessage('initialize called');
    $.pageHeader!.classList.add('connected');

    const ver = await browser.getChromeVersion();
    logMessage(`Chrome version: ${JSON.stringify(ver)}`);

    const focusedTabState = await this.browser.getFocusedTabState!();
    focusedTabState.subscribe(focusedTabChanged);
    const focusedTabStateV2 = await this.browser.getFocusedTabStateV2!();
    focusedTabStateV2.subscribe(focusedTabChangedV2);

    // Initialize permission switches and subscribe for updates.
    const permissionStates:
        Record<PermissionSwitchName, Observable<boolean>> = {
          microphone: this.browser.getMicrophonePermissionState!(),
          geolocation: this.browser.getLocationPermissionState!(),
          tabContext: this.browser.getTabContextPermissionState!(),
        };
    for (const permission of Object.keys(permissionStates) as
         PermissionSwitchName[]) {
      const state = permissionStates[permission]!;
      state.subscribe((enabled) => {
        updatePermissionSwitch(permission, enabled);
      });
    }
    browser.canAttachPanel?.().subscribe((canAttach) => {
      $.canAttachCheckbox.checked = canAttach;
    });
  }

  async notifyPanelWillOpen(panelState: PanelState):
      Promise<void|OpenPanelInfo> {
    logMessage(`notifyPanelWillOpen(${JSON.stringify(panelState)})`);
    return {
      startingMode: WebClientMode.TEXT,
      resizeParams: {
        width: pickOne([400, 500]),
        height: pickOne([400, 500]),
        options: {durationMs: pickOne([0, 1000])},
      },
    };
  }

  async notifyPanelClosed() {
    logMessage('notifyPanelClosed called');
  }
}

const client = new WebClient();

function getBrowser(): GlicBrowserHost|undefined {
  return client?.browser;
}

async function focusedTabChanged(newValue: TabData|undefined) {
  $.focusedUrl.value = '';
  $.focusedFavicon.src = '';
  logMessage(`Focused Tab State Changed: ${JSON.stringify(newValue)}`);
  if (newValue?.url) {
    $.focusedUrl.value = newValue.url;
  }
  if (newValue?.favicon) {
    const fav = await newValue.favicon();
    if (fav) {
      $.focusedFavicon.src = URL.createObjectURL(fav);
    }
  }
}

async function focusedTabChangedV2(focusedTabData: FocusedTabData|undefined) {
  $.focusedUrlV2.value = '';
  $.focusedFaviconV2.src = '';
  $.focusedTabLogsV2.innerText = '';

  if (!focusedTabData) {
    $.focusedTabLogsV2.innerText = 'Focused Tab State Changed: undefined';
    return;
  }

  if (focusedTabData.noCandidateTabError &&
      !focusedTabData.focusedTabCandidate?.invalidCandidateError) {
    $.focusedTabLogsV2.innerText = `No Candidate Tab Error: ${
        JSON.stringify(focusedTabData.noCandidateTabError)}`;
    return;
  }

  if (focusedTabData.focusedTabCandidate?.invalidCandidateError) {
    $.focusedTabLogsV2.innerText = `Focus Invalid For Extraction Error: ${
        JSON.stringify(
            focusedTabData.focusedTabCandidate.invalidCandidateError)}`;
    const candidateData =
        focusedTabData.focusedTabCandidate.focusedTabCandidateData;
    if (candidateData) {
      $.focusedUrlV2.value = candidateData.url || '';
      if (candidateData.favicon) {
        const fav = await candidateData.favicon();
        if (fav) {
          $.focusedFaviconV2.src = URL.createObjectURL(fav);
        }
      }
    }
    return;
  }

  if (focusedTabData.focusedTab) {
    const focusedTab = focusedTabData.focusedTab;
    $.focusedTabLogsV2.innerText =
        'Focused Tab State Changed: TabData available';
    $.focusedUrlV2.value = focusedTab.url || '';
    if (focusedTab.favicon) {
      const fav = await focusedTab.favicon();
      if (fav) {
        $.focusedFaviconV2.src = URL.createObjectURL(fav);
      }
    }
    return;
  }

  $.focusedTabLogsV2.innerText = 'Focused Tab State Changed: Unknown State';
}

createGlicHostRegistryOnLoad().then((registry) => {
  logMessage('registering web client');
  const params = new URLSearchParams(window.location.search);
  const delayMs = Number(params.get('delay_ms'));
  if (delayMs) {
    setTimeout(() => registry.registerWebClient(client), delayMs);
  } else {
    registry.registerWebClient(client);
  }
});

type PermissionSwitchName = 'microphone'|'geolocation'|'tabContext';
const permissionSwitches: Record<PermissionSwitchName, HTMLInputElement> = {
  microphone: $.microphoneSwitch,
  geolocation: $.geolocationSwitch,
  tabContext: $.tabContextSwitch,
};

// Update a permission switch display state.
function updatePermissionSwitch(
    permissionSwitchName: PermissionSwitchName, enabled: boolean) {
  logMessage(
      `Permission ${permissionSwitchName} updated to ${enabled}.`,
  );
  if (!permissionSwitches[permissionSwitchName]) {
    console.error('Permission switch not found: ' + permissionSwitchName);
    return;
  }
  permissionSwitches[permissionSwitchName].checked = enabled;
}

// Listen to permission update requests by the web client user.
$.testPermissionSwitch.addEventListener('click', () => {
  const selectedPermission = $.permissionSelect.value as PermissionSwitchName;
  const isEnabled = $.enabledSelect.value === 'true';
  if (!permissionSwitches[selectedPermission]) {
    console.error('Unknown permission: ' + selectedPermission);
    return;
  }
  if (selectedPermission === 'microphone') {
    getBrowser()!.setMicrophonePermissionState!(isEnabled);
  } else if (selectedPermission === 'geolocation') {
    getBrowser()!.setLocationPermissionState!(isEnabled);
  } else if (selectedPermission === 'tabContext') {
    getBrowser()!.setTabContextPermissionState!(isEnabled);
  }
  logMessage(
      `Setting permission ${selectedPermission} to ${isEnabled}.`,
  );
});

$.syncCookiesBn.addEventListener('click', async () => {
  $.syncCookieStatus!.innerText = 'Requesting';
  try {
    await getBrowser()!.refreshSignInCookies!();
    $.syncCookieStatus!.innerText = `Done!`;
  } catch (e) {
    $.syncCookieStatus!.innerText = `Caught error: ${e}`;
  }
});

$.testLogsBn.addEventListener('click', () => {
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.TEXT);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(true);
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.AUDIO);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(false);
  getBrowser()?.getMetrics?.().onSessionTerminated?.();
});

$.getUserProfileInfoBn.addEventListener('click', async () => {
  $.getUserProfileInfoStatus.innerText = 'Requesting';
  try {
    const profile = await getBrowser()!.getUserProfileInfo!();
    $.getUserProfileInfoStatus.innerText = `Done: ${JSON.stringify(profile)}`;
    const icon = await profile.avatarIcon();
    if (icon) {
      $.getUserProfileInfoImg.src = URL.createObjectURL(icon);
    }
  } catch (e) {
    $.getUserProfileInfoStatus.innerText = `Caught error: ${e}`;
  }
});

$.changeProfileBn.addEventListener('click', () => {
  getBrowser()!.showProfilePicker!();
});

// Add listeners to demo elements:
$.newtabbn.addEventListener('click', async () => {
  const url = $.URL.value;
  const openInBackground = $.createTabInBackground.checked;
  const tabData = await getBrowser()!.createTab!(url, {openInBackground});
  logMessage(`createTab done: ${JSON.stringify(tabData)}`);
});

$.reloadpage.addEventListener('click', () => {
  location.reload();
});

$.contextAccessIndicator.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicator.checked);
});

$.contextAccessIndicatorV2.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicatorV2.checked);
});

$.getpagecontext.addEventListener('click', async () => {
  logMessage('Starting Get Page Context');
  const options: any = {};
  if ($.innerTextCheckbox.checked) {
    options.innerText = true;
  }
  if ($.viewportScreenshotCheckbox.checked) {
    options.viewportScreenshot = {};
  }
  if ($.pdfDataCheckbox.checked) {
    options.pdfData = true;
  }
  if ($.annotatedPageContentCheckbox.checked) {
    options.annotatedPageContent = true;
  }
  $.faviconImg.src = '';
  $.screenshotImg.src = '';
  try {
    const pageContent =
        await client!.browser!.getContextFromFocusedTab!(options);
    if (pageContent.viewportScreenshot) {
      const blob =
          new Blob([pageContent.viewportScreenshot.data], {type: 'image/jpeg'});
      $.screenshotImg.src = URL.createObjectURL(blob);
    }
    if (pageContent.tabData.favicon) {
      const favicon = await pageContent.tabData.favicon();
      if (favicon) {
        $.faviconImg.src = URL.createObjectURL(favicon);
      }
    }
    if (pageContent.pdfDocumentData) {
      const pdfOrigin = pageContent.pdfDocumentData.origin;
      const pdfSizeLimitExceeded =
          pageContent.pdfDocumentData.pdfSizeLimitExceeded;
      let pdfDataSize = 0;
      if (pageContent.pdfDocumentData.pdfData) {
        pdfDataSize =
            (await readStream(pageContent.pdfDocumentData.pdfData!)).length;
      }
      $.getPageContextStatus.innerText =
          `Got ${pdfDataSize} bytes of PDF data(origin = ${
              pdfOrigin}, sizeLimitExceeded = ${pdfSizeLimitExceeded})`;
    }
    if (pageContent.annotatedPageData &&
        pageContent.annotatedPageData.annotatedPageContent) {
      const annotatedPageDataSize =
          (await readStream(pageContent.annotatedPageData.annotatedPageContent))
              .length;
      $.getPageContextStatus.innerText =
          `Annotated page content data length: ${annotatedPageDataSize}`;
    }
    $.getPageContextStatus.innerText =
        `Finished Get Page Context. Returned data: ${
            JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.getPageContextStatus.innerText = `Error getting page context: ${error}`;
  }
});
$.getlocation.addEventListener('click', async () => {
  logMessage('Requesting geolocation...');

  if (navigator.geolocation) {
    try {
      const position =
          await new Promise<GeolocationPosition>((resolve, reject) => {
            navigator.geolocation.getCurrentPosition(resolve, reject);
          });

      const latitude = position.coords.latitude;
      const longitude = position.coords.longitude;
      const accuracy = position.coords.accuracy;

      $.location.innerHTML = `
          Latitude: ${latitude}<br>
          Longitude: ${longitude}<br>
          Accuracy: ${accuracy} meters
        `;
      logMessage(
          `Geolocation obtained: Latitude ${latitude}, Longitude ${longitude}`);
    } catch (error) {
      if (error instanceof Error) {
        logMessage(`Error getting geolocation: ${error.message}`);
        $.location.innerHTML = `Error: ${error.message}`;
      }
    }
  } else {
    logMessage('Geolocation is not supported by this browser.');
    $.location.innerHTML = 'Geolocation is not supported by this browser.';
  }
});

$.closebn.addEventListener('click', () => {
  getBrowser()!.closePanel!();
});
$.attachpanelbn.addEventListener('click', () => {
  getBrowser()!.attachPanel!();
});
$.detachpanelbn.addEventListener('click', () => {
  getBrowser()!.detachPanel!();
});
$.refreshbn.addEventListener('click', () => {
  location.reload();
});
$.navigateWebviewUrl.addEventListener('keyup', ({key}) => {
  if (key === 'Enter') {
    window.location.href = $.navigateWebviewUrl.value;
  }
});

$.audioDuckingOn.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(true);
});

$.audioDuckingOff.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(false);
});

$.scrollToBn.addEventListener('click', async () => {
  const text = $.scrollToTextInput.value;
  if (!text) {
    logMessage('scrollTo called with empty text.');
    return;
  }
  if (!(getBrowser()!.scrollTo)) {
    logMessage(
        `scrollTo is not enabled. Run with --enable-features=GlicScrollTo.`);
    return;
  }
  logMessage(`scrollTo called with "${text}"`);
  try {
    await getBrowser()!.scrollTo!({
      selector: {exactText: {text: text}},
      highlight: true,
    });
    logMessage(`scrollTo succeeded.`);
  } catch (error) {
    logMessage(`scrollTo failed: ${error}`);
  }
});


class AudioCapture {
  recordedData: Blob[] = [];
  recorder: MediaRecorder|undefined;
  constructor() {}

  async start() {
    if (this.recorder) {
      return;
    }
    const stream = await navigator.mediaDevices.getUserMedia({audio: true});

    $.audioStatus.replaceChildren('Recording...');
    this.recorder = new MediaRecorder(stream, {mimeType: 'audio/webm'});
    let stopped = false;
    window.setInterval(() => {
      if (!stopped) {
        this.recorder!.requestData();
      }
    }, 100);
    this.recorder.addEventListener('dataavailable', (event: BlobEvent) => {
      this.recordedData.push(event.data);
    });
    this.recorder.addEventListener('stop', () => {
      stopped = true;
      $.audioStatus.replaceChildren('Playback...');
      const blob = new Blob(this.recordedData, {type: 'audio/webm'});
      $.mic.src = URL.createObjectURL(blob);
    });
    this.recorder.start();
  }

  stop() {
    if (!this.recorder) {
      return;
    }
    $.mic.play();
    this.recorder.stop();
    this.recorder = undefined;
  }
}
const audioCapture = new AudioCapture();

window.addEventListener('load', () => {
  $.audioCapStop.addEventListener('click', () => {
    audioCapture.stop();
  });
  $.audioCapStart.addEventListener('click', () => {
    audioCapture.start();
  });
  $.desktopScreenshot.addEventListener('click', async () => {
    logMessage('Requesting desktop screenshot...');
    try {
      const screenshot = await getBrowser()!.captureScreenshot!();
      if (screenshot) {
        const blob = new Blob([screenshot.data], {type: 'image/jpeg'});
        $.desktopScreenshotImg.src = URL.createObjectURL(blob);
        $.desktopScreenshotErrorReason!.innerText =
            'Desktop screenshot captured.';
      } else {
        $.desktopScreenshotErrorReason!.innerText =
            'Failed to capture desktop screenshot.';
      }
    } catch (error) {
      $.desktopScreenshotErrorReason!.innerText = `Caught error: ${error}`;
    }
  });
});

function readStream(stream: ReadableStream<Uint8Array>): Promise<Uint8Array> {
  return new Response(stream).bytes();
}

function pickOne(choices: any[]): any {
  return choices[Math.floor(Math.random() * choices.length)];
}
