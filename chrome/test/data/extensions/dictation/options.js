// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function saveOptions() {
  const cannedResponse = document.getElementById('cannedResponse').value;
  const wordDelay = Number(document.getElementById('wordDelay').value);
  const finalDelay = Number(document.getElementById('finalDelay').value);

  await chrome.storage.local.set({cannedResponse, wordDelay, finalDelay});
}

async function restoreOptions() {
  const optionsItems = await chrome.storage.local.get(
      {cannedResponse: '', wordDelay: 0, finalDelay: 0});

  document.getElementById('cannedResponse').value = optionsItems.cannedResponse;
  document.getElementById('wordDelay').value = optionsItems.wordDelay;
  document.getElementById('finalDelay').value = optionsItems.finalDelay;
}

document.addEventListener('DOMContentLoaded', restoreOptions);
document.getElementById('cannedResponse')
    .addEventListener('change', saveOptions);
document.getElementById('wordDelay').addEventListener('change', saveOptions);
document.getElementById('finalDelay').addEventListener('change', saveOptions);
