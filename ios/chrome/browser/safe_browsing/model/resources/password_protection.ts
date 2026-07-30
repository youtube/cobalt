// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/*
 * @fileoverview Adds listeners that forward keydown and paste events to the
 * browser. The browser uses this information to detect and warn the user about
 * situations where the user enters one of their saved passwords on a
 * possibly-unsafe site
 */

/**
 * Returns true if the target is an editable element.
 */
function isEditableTarget(target: EventTarget|null): boolean {
  if (!target) {
    return false;
  }
  if (target instanceof HTMLInputElement ||
      target instanceof HTMLTextAreaElement) {
    return true;
  }
  if (target instanceof HTMLElement && target.isContentEditable) {
    return true;
  }
  return false;
}

/**
 * Returns the actual event target, resolving target inside Shadow DOM.
 */
function getRealTarget(event: Event): EventTarget|null {
  return (event.composedPath && event.composedPath().length > 0) ?
      (event.composedPath()[0] || null) :
      event.target;
}

/**
 * Listens for keydown events and forwards the entered key to the browser.
 */
function onKeydownEvent(event: KeyboardEvent): void {
  if (!event.isTrusted) {
    return;
  }

  // Only forward events where the entered key has length 1, to avoid
  // forwarding special keys like "Enter".
  if ([...event.key].length === 1 && !event.ctrlKey && !event.metaKey) {
    sendWebKitMessage(
        'PasswordProtectionTextEntered',
        {eventType: 'KeyDown', text: event.key});
  } else if (
      (event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'v') {
    if (isEditableTarget(getRealTarget(event))) {
      sendWebKitMessage(
          'PasswordProtectionTextEntered',
          {eventType: 'PasteKeyDetected', text: ''});
    }
  }
}

/**
 * Listens for paste events and forwards the pasted text to the browser.
 */
function onPasteEvent(event : Event) : void {
  if (!(event instanceof ClipboardEvent)) {
    return;
  }

  if (!event.isTrusted) {
    return;
  }

  const clipboardData = event.clipboardData;
  if (!clipboardData) {
    return;
  }

  const text = clipboardData.getData('text');
  sendWebKitMessage(
      'PasswordProtectionTextEntered',
      {eventType: 'TextPasted', text: text});
}

// Events are first dispatched to the window object, in the capture phase of
// JavaScript event dispatch, so listen for them there.
window.addEventListener('keydown', onKeydownEvent, true);
window.addEventListener('paste', onPasteEvent, true);
