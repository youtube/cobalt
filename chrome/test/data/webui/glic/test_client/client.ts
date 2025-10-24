// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FocusedTabData, GlicBrowserHost, GlicWebClient, Observable, OpenPanelInfo, PanelOpeningData, PanelState, TabData, WebClientInitializeError} from '/glic/glic_api/glic_api.js';
import {WebClientInitializeErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';

import {$} from './page_element_types.js';

export function logMessage(message: string) {
  const d = new Date();
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  const ss = String(d.getSeconds()).padStart(2, '0');
  const ms = String(d.getMilliseconds()).padStart(3, '0');
  const timeStamp = `${hh}:${mm}:${ss}.${ms}: `;
  $.status.append(
      timeStamp, message.slice(0, 100000), document.createElement('br'));
}

export function readStream(stream: ReadableStream<Uint8Array>):
    Promise<Uint8Array> {
  return new Response(stream).bytes();
}

export function getBrowser(): GlicBrowserHost|undefined {
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

  if (focusedTabData.hasNoFocus) {
    $.focusedTabLogsV2.innerText = `No focus reason: ${
        focusedTabData.hasNoFocus.noFocusReason} active tab url: ${
        focusedTabData.hasNoFocus.tabFocusCandidateData?.url}`;
    return;
  }

  if (focusedTabData.hasFocus) {
    const focusedTab = focusedTabData.hasFocus.tabData;
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

class TestInitFailure extends Error implements WebClientInitializeError {
  reason = WebClientInitializeErrorReason.UNKNOWN;
  readonly reasonType = 'webClientInitialize';
  constructor() {
    super('test-init-failure');
  }
}

export type PermissionSwitchName =
    'microphone'|'geolocation'|'tabContext'|'osGeolocation';
export const permissionSwitches:
    Record<PermissionSwitchName, HTMLInputElement> = {
      microphone: $.microphoneSwitch,
      geolocation: $.geolocationSwitch,
      tabContext: $.tabContextSwitch,
      osGeolocation: $.osGeolocationPermissionSwitch,
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

class WebClient implements GlicWebClient {
  browser: GlicBrowserHost|undefined;

  async initialize(browser: GlicBrowserHost): Promise<void> {
    if (localStorage.getItem('test-init-failure')) {
      localStorage.removeItem('test-init-failure');
      throw new TestInitFailure();
    }
    this.browser = browser;

    logMessage('initialize called');
    $.pageHeader!.classList.add('connected');

    // Disable sections with unavailable functionality.
    if (this.browser.openOsPermissionSettingsMenu === undefined) {
      logMessage('OS permissions are disabled');
      $.macOsPermissionsFieldset.disabled = true;
    }
    if (this.browser.attachPanel === undefined) {
      logMessage('Attachment controls are disabled (detached-only mode)');
      $.attachmentControlsFieldset.disabled = true;
    }

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
          osGeolocation: this.browser.getOsLocationPermissionState!(),
        };
    for (const permission of Object.keys(permissionStates) as
         PermissionSwitchName[]) {
      const state = permissionStates[permission]!;
      state.subscribe((enabled) => {
        updatePermissionSwitch(permission, enabled);
      });
    }
    const closedCaptioningState =
        await this.browser.getClosedCaptioningSetting?.();
    closedCaptioningState?.subscribe((enabled) => {
      $.closedCaptioningSwitch.checked = enabled;
    });
    browser.canAttachPanel?.().subscribe((canAttach) => {
      $.canAttachCheckbox.checked = canAttach;
    });
    browser.panelActive?.().subscribe((active) => {
      $.panelActiveCheckbox.checked = active;
    });
    browser.isManuallyResizing?.().subscribe((resizing) => {
      logMessage('Manually resizing state changed: ' + resizing);
    });
    if (browser.getOsHotkeyState) {
      const hotkeyState = await browser.getOsHotkeyState();
      hotkeyState.subscribe((data: {hotkey: string}) => {
        $.osGlicHotkey.value = data.hotkey === '' ? 'Not Set' : data.hotkey;
      });
    } else {
      logMessage('getOsHotkeyState not available');
    }
    $.enableDragResizeCheckbox.disabled =
        browser.enableDragResize === undefined;
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningData&PanelState):
      Promise<OpenPanelInfo> {
    function pickOne(choices: any[]): any {
      return choices[Math.floor(Math.random() * choices.length)];
    }

    // Deleting backwards-compatible members coming from PanelState.
    delete (panelOpeningData as Partial<PanelState>).kind;
    delete (panelOpeningData as Partial<PanelState>).windowId;
    logMessage(`notifyPanelWillOpen(${JSON.stringify(panelOpeningData)})`);
    this.browser!.setContextAccessIndicator!
        ($.contextAccessIndicatorV2.checked);

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

  async checkResponsive() {
    // Nothing need to be checked on the test client.
  }
}

export const client = new WebClient();

// This allows browser tests using this test client to be able to access and
// call the glic API directly (using ExecuteJs and similar methods).
declare global {
  interface Window {
    client: WebClient;
  }
}
window.client = client;
