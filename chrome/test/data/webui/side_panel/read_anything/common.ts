// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {MetricsBrowserProxyImpl, NodeStore, playFromSelectionTimeout, ReadAnythingLogger, ToolbarEvent, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import type {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

export async function createApp(): Promise<AppElement> {
  const app = document.createElement('read-anything-app');
  document.body.appendChild(app);
  await microtasksFinished();
  return app;
}

export function mockMetrics(): TestMetricsBrowserProxy {
  const metrics = new TestMetricsBrowserProxy();
  MetricsBrowserProxyImpl.setInstance(metrics);
  ReadAnythingLogger.setInstance(new ReadAnythingLogger());
  return metrics;
}

export function emitEvent(app: AppElement, name: string, options?: any): void {
  app.$.toolbar.dispatchEvent(new CustomEvent(name, options));
}

// Runs the requestAnimationFrame callback immediately
export function stubAnimationFrame() {
  window.requestAnimationFrame = (callback) => {
    callback(0);
    return 0;
  };
}

export function playFromSelectionWithMockTimer(app: AppElement): void {
  const mockTimer = new MockTimer();
  mockTimer.install();
  emitEvent(app, ToolbarEvent.PLAY_PAUSE);
  mockTimer.tick(playFromSelectionTimeout);
  mockTimer.uninstall();
}

// Returns the list of items in the given dropdown menu
export function getItemsInMenu(
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>):
    HTMLButtonElement[] {
  // We need to call menu.get here to ensure the menu has rendered before we
  // query the dropdown item elements.
  const menu = lazyMenu.get();
  return Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
}

export function createSpeechErrorEvent(
    utterance: SpeechSynthesisUtterance,
    errorCode: SpeechSynthesisErrorCode): SpeechSynthesisErrorEvent {
  return new SpeechSynthesisErrorEvent(
      'type', {utterance: utterance, error: errorCode});
}

export function setupBasicSpeech(speech: TestSpeechBrowserProxy) {
  VoiceLanguageController.getInstance().enableLang('en');
  createAndSetVoices(
      speech, [{lang: 'en', name: 'Google Basic', default: true}]);
}

// Creates SpeechSynthesisVoices and sets them on the given
// TestSpeechBrowserProxy.
export function createAndSetVoices(
    speech: TestSpeechBrowserProxy,
    overrides: Array<Partial<SpeechSynthesisVoice>>) {
  const voices: SpeechSynthesisVoice[] = [];
  overrides.forEach(partialVoice => {
    voices.push(createSpeechSynthesisVoice(partialVoice));
  });
  setVoices(speech, voices);
}

export function setVoices(
    speech: TestSpeechBrowserProxy, voices: SpeechSynthesisVoice[]) {
  speech.setVoices(voices);
  VoiceLanguageController.getInstance().onVoicesChanged();
}

export function createSpeechSynthesisVoice(
    overrides?: Partial<SpeechSynthesisVoice>): SpeechSynthesisVoice {
  return Object.assign(
      {
        default: false,
        name: '',
        lang: 'en-us',
        localService: false,
        voiceURI: '',
      },
      overrides || {});
}


export function setSimpleAxTreeWithText(text: string) {
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2],
      },
      {id: 2, role: 'staticText', name: text},
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2]);
}

export function setSimpleNodeStoreWithText(text: string) {
  const id = 2;
  chrome.readingMode.getCurrentText = () => [id];
  chrome.readingMode.getTextContent = () => text;
  chrome.readingMode.getCurrentTextStartIndex = () => 0;
  chrome.readingMode.getCurrentTextEndIndex = () => text.length;
  NodeStore.getInstance().setDomNode(document.createTextNode(text), id);
}
