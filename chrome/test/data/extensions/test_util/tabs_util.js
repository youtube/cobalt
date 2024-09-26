// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Opens a new tab and waits for it to finish loading.
 * @param {string} url The url for the new tab to point to.
 * @return {!Promise<tabs.Tab>} A promise that resolves when the tab has
 *     finished loading, with the value of the tab.
 */
export function openTab(url) {
  return new Promise((resolve) => {
    let createdTabId;
    let completedTabIds = [];
    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
      if (changeInfo.status !== 'complete')
        return;  // Tab not done.

      if (createdTabId === undefined) {
        // A tab completed loading before the chrome.tabs.create callback was
        // triggered; stash the ID for later comparison to see if it was our
        // tab.
        completedTabIds.push(tabId);
        return;
      }

      if (tabId !== createdTabId)
        return;  // Not our tab.

      // It's ours!
      chrome.tabs.onUpdated.removeListener(listener);
      resolve(tab);
    });
    chrome.tabs.create({url: url}, (tab) => {
      if (completedTabIds.includes(tab.id))
        resolve(tab);
      else
        createdTabId = tab.id;
    });
  });
}
