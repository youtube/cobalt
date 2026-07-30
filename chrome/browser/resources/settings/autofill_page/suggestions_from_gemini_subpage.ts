// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_subpage.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './your_saved_info_shared.css.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './suggestions_from_gemini_subpage.html.js';

const SettingsSuggestionsFromGeminiSubpageElementBase =
    SettingsViewMixin(PolymerElement);

export class SettingsSuggestionsFromGeminiSubpageElement extends
    SettingsSuggestionsFromGeminiSubpageElementBase {
  static get is() {
    return 'settings-suggestions-from-gemini-subpage';
  }

  static get template() {
    return getTemplate();
  }

  private onManageConnectedAppsClick_() {
    // TODO(crbug.com/512204278): Add metrics.
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('personalContextConnectedAppsUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-suggestions-from-gemini-subpage':
        SettingsSuggestionsFromGeminiSubpageElement;
  }
}

customElements.define(
    SettingsSuggestionsFromGeminiSubpageElement.is,
    SettingsSuggestionsFromGeminiSubpageElement);
