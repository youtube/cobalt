// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The handle* functions below are called internally on promise
// resolution, unlike the other return* functions, which are called
// asynchronously by the host.

// clang-format off
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';
// <if expr="chromeos_ash or is_win or is_chromeos">
import {addWebUiListener} from 'chrome://resources/js/cr.js';
// </if>
// <if expr="is_chromeos">
import {$} from 'chrome://resources/js/util_ts.js';
// </if>
// clang-format on

/**
 * Promise resolution handler for variations list and command line equivalent.
 */
function handleVariationInfo(
    {variationsList, variationsCmd}:
        {variationsList: string[], variationsCmd?: string}) {
  getRequiredElement('variations-section').hidden = !variationsList.length;
  for (const item of variationsList) {
    getRequiredElement('variations-list')
        .appendChild(document.createTextNode(item));
    getRequiredElement('variations-list')
        .appendChild(document.createElement('br'));
  }

  if (variationsCmd) {
    getRequiredElement('variations-cmd-section').hidden = !variationsCmd;
    getRequiredElement('variations-cmd').textContent = variationsCmd;
  }
}

/**
 * Promise resolution handler for the executable and profile paths to display.
 * @param execPath The executable path to display.
 * @param profilePath The profile path to display.
 */
function handlePathInfo(
    {execPath, profilePath}: {execPath: string, profilePath: string}) {
  getRequiredElement('executable_path').textContent = execPath;
  getRequiredElement('profile_path').textContent = profilePath;
}

// <if expr="chromeos_ash or is_win">
/**
 * Callback from the backend with the OS version to display.
 * @param osVersion The OS version to display.
 */
function returnOsVersion(osVersion: string) {
  getRequiredElement('os_version').textContent = osVersion;
}
// </if>

// <if expr="chromeos_ash">
/**
 * Callback from the backend with the firmware version to display.
 */
function returnOsFirmwareVersion(firmwareVersion: string) {
  getRequiredElement('firmware_version').textContent = firmwareVersion;
}

/**
 * Callback from the backend with the ARC Android SDK version to display.
 * @param arcAndroidSdkVersion The ARC Android SDK version to display,
 *     already localized.
 */
function returnArcAndArcAndroidSdkVersions(arcAndroidSdkVersion: string) {
  getRequiredElement('arc_holder').hidden = false;
  getRequiredElement('arc_and_arc_android_sdk_versions').textContent =
      arcAndroidSdkVersion;
}

/**
 * Callback from chromeosInfoPrivate with the value of the customization ID.
 */
function returnCustomizationId(response: {[customizationId: string]: any}) {
  if (!response['customizationId']) {
    return;
  }
  getRequiredElement('customization_id_holder').hidden = false;
  getRequiredElement('customization_id').textContent =
      response['customizationId'];
}
// </if>

// <if expr="is_chromeos">
/**
 * Callback from the backend to inform if Lacros is primary or not.
 * @param isPrimary True if it is primary.
 */
function returnLacrosPrimary(isPrimary: string) {
  getRequiredElement('os-link-container').hidden = !isPrimary;

  const crosUrlRedirectButton = $('os-link-href');
  if (crosUrlRedirectButton) {
    crosUrlRedirectButton.onclick = crosUrlVersionRedirect;
  }
}

/**
 * Called when the user clicks on the os-link-href button.
 */
function crosUrlVersionRedirect() {
  chrome.send('crosUrlVersionRedirect');
}

// </if>

function copyToClipboard() {
  navigator.clipboard.writeText(getRequiredElement('copy-content').innerText);
}

// <if expr="chromeos_lacros">
function copyOSContentToClipboard() {
  navigator.clipboard.writeText(
      getRequiredElement('copy-os-content').innerText);
}
// </if>

/* All the work we do onload. */
function initialize() {
  // <if expr="chromeos_ash or is_win">
  addWebUiListener('return-os-version', returnOsVersion);
  // </if>
  // <if expr="chromeos_ash">
  addWebUiListener('return-os-firmware-version', returnOsFirmwareVersion);
  addWebUiListener(
      'return-arc-and-arc-android-sdk-versions',
      returnArcAndArcAndroidSdkVersions);
  // </if>
  // <if expr="is_chromeos">
  addWebUiListener('return-lacros-primary', returnLacrosPrimary);
  // </if>

  chrome.send('requestVersionInfo');
  const includeVariationsCmd = location.search.includes('show-variations-cmd');
  sendWithPromise('requestVariationInfo', includeVariationsCmd)
      .then(handleVariationInfo);
  sendWithPromise('requestPathInfo').then(handlePathInfo);

  // <if expr="chromeos_ash">
  getRequiredElement('arc_holder').hidden = true;
  chrome.chromeosInfoPrivate.get(['customizationId'])
      .then(returnCustomizationId);
  // </if>

  if (getRequiredElement('variations-seed').textContent !== '') {
    getRequiredElement('variations-seed-section').hidden = false;
  }

  if (getRequiredElement('sanitizer').textContent !== '') {
    getRequiredElement('sanitizer-section').hidden = false;
  }

  getRequiredElement('copy-to-clipboard')
      .addEventListener('click', copyToClipboard);

  // <if expr="chromeos_lacros">
  getRequiredElement('copy-os-content-to-clipboard')
      .addEventListener('click', copyOSContentToClipboard);
  // </if>
}

document.addEventListener('DOMContentLoaded', initialize);
