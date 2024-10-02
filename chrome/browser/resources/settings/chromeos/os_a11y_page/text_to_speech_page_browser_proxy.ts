// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface TextToSpeechPageBrowserProxy {
  /**
   * Request whether ScreenAIInstallState changed. Result is returned by the
   * 'pdf-ocr-state-changed' WebUI listener event.
   */
  pdfOcrSectionReady(): void;

  /**
   * Opens the options page for Chrome Vox.
   */
  showChromeVoxSettings(): void;

  /**
   * Opens the options page for select to speak.
   */
  showSelectToSpeakSettings(): void;

  /**
   * Opens the ChromeVox tutorial.
   */
  showChromeVoxTutorial(): void;
}

let instance: TextToSpeechPageBrowserProxy|null = null;

export class TextToSpeechPageBrowserProxyImpl implements
    TextToSpeechPageBrowserProxy {
  static getInstance(): TextToSpeechPageBrowserProxy {
    return instance || (instance = new TextToSpeechPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: TextToSpeechPageBrowserProxy): void {
    instance = obj;
  }

  pdfOcrSectionReady(): void {
    chrome.send('pdfOcrSectionReady');
  }

  showChromeVoxSettings(): void {
    chrome.send('showChromeVoxSettings');
  }

  showSelectToSpeakSettings(): void {
    chrome.send('showSelectToSpeakSettings');
  }

  showChromeVoxTutorial(): void {
    chrome.send('showChromeVoxTutorial');
  }
}
