// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const OFFSCREEN_PATH = 'offscreen.html';

async function isManualTest() {
  const options = await chrome.storage.local.get({manualTest: false});
  return options.manualTest;
}

async function hasOffscreenDocument() {
  const offscreenUrl = chrome.runtime.getURL(OFFSCREEN_PATH);
  const existingContexts = await chrome.runtime.getContexts({
    contextTypes: ['OFFSCREEN_DOCUMENT'],
    documentUrls: [offscreenUrl],
  });

  return existingContexts.length > 0;
}

let creatingDocument = null;
async function setupOffscreenDocument() {
  if (await hasOffscreenDocument()) {
    return;
  }

  if (creatingDocument) {
    await creatingDocument;
  } else {
    creatingDocument = chrome.offscreen.createDocument({
      url: OFFSCREEN_PATH,
      reasons: ['USER_MEDIA'],
      justification: 'Dictation test extension requires microphone access',
    });
    await creatingDocument;
    creatingDocument = null;
  }
}

async function startStream(streamId) {
  chrome.dictationPrivate.setStreamState(
      {streamId, state: chrome.dictationPrivate.StreamState.INITIALIZING});

  await setupOffscreenDocument();
  const optionsItems = await chrome.storage.local.get(
      {cannedResponse: '', wordDelay: 0, finalDelay: 0});
  const startResult = await chrome.runtime.sendMessage({
    command: 'startStream',
    streamId,
    cannedResponse: optionsItems.cannedResponse,
    wordDelay: optionsItems.wordDelay,
    finalDelay: optionsItems.finalDelay,
  });
  chrome.dictationPrivate.setStreamState({
    streamId,
    state: (startResult.status === 'started') ?
        chrome.dictationPrivate.StreamState.TRANSCRIBING :
        chrome.dictationPrivate.StreamState.FAILED,
  });
}

async function endStream(streamId) {
  if (!await hasOffscreenDocument()) {
    return;
  }

  chrome.runtime.sendMessage({command: 'endStream', streamId});
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  const {streamId, type, data, error} = message;

  console.info(
      `[Dictation Result] type: "${type}", data: "${data}", error: "${error}"`);

  if (error) {
    chrome.dictationPrivate.setStreamState(
        {streamId, state: chrome.dictationPrivate.StreamState.FAILED});
  } else if (
      type === chrome.dictationPrivate.TranscriptionType.FINAL ||
      type === chrome.dictationPrivate.TranscriptionType.PARTIAL) {
    chrome.dictationPrivate.updateTranscription({streamId, type, data});
  }
});

chrome.dictationPrivate.onStartStream.addListener(async (details) => {
  // In a manual test, the test code itself simulates the extension API calls
  // so avoid calling into any of the extension code.
  if (await isManualTest()) {
    return;
  }

  startStream(details.streamId);
});

chrome.dictationPrivate.onEndStream.addListener(async (details) => {
  // In a manual test, the test code itself simulates the extension API calls
  // so avoid calling into any of the extension code.
  if (await isManualTest()) {
    return;
  }

  const {streamId} = details;

  await endStream(streamId);
  chrome.dictationPrivate.setStreamState(
      {streamId, state: chrome.dictationPrivate.StreamState.COMPLETE});
});
