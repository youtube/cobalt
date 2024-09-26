// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * strings.m.js is generated when we enable it via UseStringsJs() in webUI
 * controller. When loading it, it will populate data such as localized strings
 * into |window.loadTimeData|.
 */
import '/strings.m.js';

import {AppWindow} from './app_window.js';
import * as Comlink from './lib/comlink.js';
import {TestBridge} from './test_bridge.js';
import {
  createUntrustedIframe,
  injectUntrustedJSModule,
  setGAHelper,
  setVideoProcessorHelper,
} from './untrusted_scripts.js';


declare global {
  interface Window {
    // TODO(crbug.com/980846): Refactor to use a better way rather than window
    // properties to pass data to other modules.
    appWindow: Comlink.Remote<AppWindow>|null;
    windowCreationTime: number;
  }
}


document.addEventListener('DOMContentLoaded', async () => {
  const workerPath = '/js/test_bridge.js';
  const sharedWorker = new SharedWorker(workerPath, {type: 'module'});
  const testBridge = Comlink.wrap<TestBridge>(sharedWorker.port);

  // To support code coverage collection and communication with tast, the
  // initialization is split into several steps:
  // 1. Creates all untrusted iframes.
  // 2. Binds the appWindow used for communication with tast if running under
  //    tast, which also tells tast that CCA is ready.
  // 3. Tast attaches to the main page and all untrusted iframes, and start
  //    profiling to collect code coverage. CCA waits for tast with
  //    waitUntilReadyOnTastSide.
  // 4. Injects the untrusted scripts into the untrusted iframes now, so the
  //    code coverage of those untrusted scripts will be collected.
  // 5. Load the error.js and main.js.
  // TODO(b/197712141): Also creates all dedicated worker and attach them on
  // tast side to collect code coverage.

  // TODO(pihsun): Currently the untrusted_script_loader.ts will not be
  // included in coverage because it can be run before we can start profiling.
  // We might be able to use Target.autoAttachRelated and
  // waitForDebuggerOnStart in CDP to achieve this without separating the steps
  // here, but those are currently limited to browser target and not usable by
  // tast now.
  const gaHelperIFrame = createUntrustedIframe();
  const videoProcessorHelperIFrame = createUntrustedIframe();

  const appWindow = await testBridge.bindWindow(window.location.href);
  window.appWindow = appWindow;
  window.windowCreationTime = performance.now();
  if (appWindow !== null) {
    await appWindow.waitUntilReadyOnTastSide();
  }

  setGAHelper(
      injectUntrustedJSModule(gaHelperIFrame, '/js/untrusted_ga_helper.js'));
  setVideoProcessorHelper(injectUntrustedJSModule(
      videoProcessorHelperIFrame, '/js/untrusted_video_processor_helper.js'));

  // Dynamically import the error module here so that the codes can be counted
  // by coverage report.
  const errorModule = await import('./error.js');
  errorModule.initialize();

  const mainScript = document.createElement('script');
  mainScript.setAttribute('type', 'module');
  mainScript.setAttribute('src', '/js/main.js');
  document.head.appendChild(mainScript);
});
