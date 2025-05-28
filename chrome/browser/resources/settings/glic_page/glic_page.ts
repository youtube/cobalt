// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import type {GlicBrowserProxy} from './glic_browser_proxy.js';
import {GlicBrowserProxyImpl} from './glic_browser_proxy.js';
import {getTemplate} from './glic_page.html.js';

export enum SettingsGlicPageFeaturePrefName {
  SETTINGS_POLICY = 'glic.settings_policy',
  LAUNCHER_ENABLED = 'glic.launcher_enabled',

}

// browser_element_identifiers constants
const OS_WIDGET_TOGGLE_ELEMENT_ID = 'kGlicOsToggleElementId';
const OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID =
    'kGlicOsWidgetKeyboardShortcutElementId';

const SettingsGlicPageElementBase = HelpBubbleMixin(PrefsMixin(PolymerElement));

export class SettingsGlicPageElement extends SettingsGlicPageElementBase {
  static get is() {
    return 'settings-glic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      registeredShortcut_: {
        type: String,
        value: '',
      },

      // When the policy is disabled, the controls need to all show "off" so we
      // render a page with all the toggles bound to this fake pref rather than
      // real pref which could be either value.
      fakePref_: {
        type: Object,
        value: {
          key: 'glic.fake_pref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: 0,
        },
      },
    };
  }

  private registeredShortcut_: string;
  private fakePref_: chrome.settingsPrivate.PrefObject;
  private browserProxy_: GlicBrowserProxy = GlicBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
  }

  private async onEnabledTemplateDomChange_() {
    await CrSettingsPrefs.initialized;
    if (!this.isEnabledByPolicy_()) {
      return;
    }

    const launcherToggle =
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#launcherToggle');
    const shortcutInput =
        this.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#shortcutInput');
    assert(launcherToggle);
    assert(shortcutInput);

    this.registerHelpBubble(
        OS_WIDGET_TOGGLE_ELEMENT_ID, launcherToggle.getBubbleAnchor());
    this.registerHelpBubble(
        OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID, shortcutInput);
  }

  private onToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.browserProxy_.setGlicOsLauncherEnabled(enabled);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Glic.OsEntrypoint.Settings.Toggle', enabled);
    this.hideHelpBubble(OS_WIDGET_TOGGLE_ELEMENT_ID);
  }

  private async onShortcutUpdated_(event: CustomEvent<string>) {
    await this.browserProxy_.setGlicShortcut(event.detail);
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
    // Records true if the shortcut string is not undefined or the empty string.
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Glic.OsEntrypoint.Settings.Shortcut', !!event.detail);
    this.hideHelpBubble(OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID);
  }

  private onInputCaptureChange_(event: CustomEvent<boolean>) {
    this.browserProxy_.setShortcutSuspensionState(event.detail);
  }

  private shouldShowKeyboardShortcut_(launcherEnabled: boolean): boolean {
    return this.isEnabledByPolicy_() && launcherEnabled;
  }

  private isEnabledByPolicy_(): boolean {
    return this.getPref<number>(SettingsGlicPageFeaturePrefName.SETTINGS_POLICY)
               .value === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-page': SettingsGlicPageElement;
  }
}

customElements.define(SettingsGlicPageElement.is, SettingsGlicPageElement);
