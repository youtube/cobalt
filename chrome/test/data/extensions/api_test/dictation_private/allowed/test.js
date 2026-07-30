// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (chrome.dictationPrivate === undefined) {
  console.error('chrome.dictationPrivate is undefined');
  chrome.test.sendMessage('failed');
} else {
  chrome.dictationPrivate.onStartStream.addListener(async (details) => {
    const {streamId, pageContext, editableContent} = details;
    chrome.test.assertEq(123, streamId);
    chrome.test.assertEq('Page context', pageContext);
    chrome.test.assertEq('Existing content', editableContent);

    chrome.test.assertTrue(
        typeof chrome.dictationPrivate.updateTranscription === 'function');
    chrome.test.assertTrue(
        typeof chrome.dictationPrivate.setStreamState === 'function');

    try {
      await chrome.dictationPrivate.setStreamState({
        streamId,
        state: chrome.dictationPrivate.StreamState.TRANSCRIBING,
      });
      await chrome.dictationPrivate.updateTranscription({
        streamId,
        type: chrome.dictationPrivate.TranscriptionType.PARTIAL,
        data: 'Hello',
      });
      await chrome.dictationPrivate.updateTranscription({
        streamId,
        type: chrome.dictationPrivate.TranscriptionType.FINAL,
        data: 'Hello world',
      });
      await chrome.dictationPrivate.setStreamState({
        streamId,
        state: chrome.dictationPrivate.StreamState.COMPLETE,
      });
    } catch (err) {
      chrome.test.fail('Failed calling functions on namespace: ' + err);
      return;
    }

    chrome.test.succeed();
  });

  chrome.test.sendMessage('ready');
}
