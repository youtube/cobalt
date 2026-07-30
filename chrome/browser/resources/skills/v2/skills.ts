// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {getRequiredElement} from '//resources/js/util.js';

import {ErrorType} from '../error_page.js';
import {SkillsPageHandler} from '../skills.mojom-webui.js';

const handler = SkillsPageHandler.getRemote();

async function init() {
  // Wait for cookie sync to complete before setting src
  const {success} = await handler.syncCookies();
  if (!success) {
    const errorPage = document.querySelector('error-page');
    if (errorPage) {
      errorPage.errorType = ErrorType.GLIC_NOT_ENABLED;
      errorPage.removeAttribute('hidden');
    }

    // Hide the webview
    const webview = getRequiredElement('webview');
    webview.setAttribute('hidden', 'true');
    return;
  }
}

init();
