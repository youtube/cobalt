// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {createLogsMapTable} from './logs_map_page.js';

const dialogArgs: string = chrome.getVariableValue('dialogArguments');

/**
 * Builds the autofill metadata table. Constructs the map entries for the logs
 * page by parsing the input json to readable string.
 */
function createAutofillMetadataTable() {
  const autofillMetadata = JSON.parse(dialogArgs);

  const items: chrome.feedbackPrivate.LogsMapEntry[] = [];

  if (autofillMetadata.triggerFormSignature) {
    items.push({
      key: 'trigger_form_signature',
      value: autofillMetadata.triggerFormSignature,
    });
  }

  if (autofillMetadata.triggerFieldSignature) {
    items.push({
      key: 'trigger_field_signature',
      value: autofillMetadata.triggerFieldSignature,
    });
  }

  if (autofillMetadata.lastAutofillEvent) {
    items.push({
      key: 'last_autofill_event',
      value: JSON.stringify(autofillMetadata.lastAutofillEvent, null, 1),
    });
  }

  if (autofillMetadata.formStructures) {
    items.push({
      key: 'form_structures',
      value: JSON.stringify(autofillMetadata.formStructures, null, 1),
    });
  }

  createLogsMapTable(items);
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  if (!dialogArgs) {
    return;
  }

  createAutofillMetadataTable();
};
