// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {binaryDecode} from '../base/string_utils';
import {ChromeTracingController} from './chrome_tracing_controller';

<<<<<<< HEAD
let chromeTraceController: ChromeTracingController | undefined = undefined;
=======
let chromeTraceController: ChromeTracingController|undefined = undefined;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

enableOnlyOnPerfettoHost();

// Listen for messages from the perfetto ui.
<<<<<<< HEAD
// eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
if (globalThis.chrome) {
=======
if (window.chrome) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  chrome.runtime.onConnectExternal.addListener((port) => {
    chromeTraceController = new ChromeTracingController(port);
    port.onMessage.addListener(onUIMessage);
  });
}

function onUIMessage(
<<<<<<< HEAD
  message: {method: string; requestData: string},
  port: chrome.runtime.Port,
) {
=======
    message: {method: string, requestData: string}, port: chrome.runtime.Port) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  if (message.method === 'ExtensionVersion') {
    port.postMessage({version: chrome.runtime.getManifest().version});
    return;
  }
  console.assert(chromeTraceController !== undefined);
  if (!chromeTraceController) return;
  // ChromeExtensionConsumerPort sends the request data as string because
  // chrome.runtime.port doesn't support ArrayBuffers.
<<<<<<< HEAD
  const requestDataArray: Uint8Array = message.requestData
    ? binaryDecode(message.requestData)
    : new Uint8Array();
=======
  const requestDataArray: Uint8Array = message.requestData ?
      binaryDecode(message.requestData) :
      new Uint8Array();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  chromeTraceController.handleCommand(message.method, requestDataArray);
}

function enableOnlyOnPerfettoHost() {
  function enableOnHostWithSuffix(suffix: string) {
    return {
<<<<<<< HEAD
      conditions: [
        new chrome.declarativeContent.PageStateMatcher({
          pageUrl: {hostSuffix: suffix},
        }),
      ],
=======
      conditions: [new chrome.declarativeContent.PageStateMatcher({
        pageUrl: {hostSuffix: suffix},
      })],
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      actions: [new chrome.declarativeContent.ShowPageAction()],
    };
  }
  chrome.declarativeContent.onPageChanged.removeRules(undefined, () => {
    chrome.declarativeContent.onPageChanged.addRules([
      enableOnHostWithSuffix('localhost'),
      enableOnHostWithSuffix('127.0.0.1'),
      enableOnHostWithSuffix('.perfetto.dev'),
      enableOnHostWithSuffix('.storage.googleapis.com'),
    ]);
  });
}
