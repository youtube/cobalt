// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard' is the settings subpage for keyboard settings.
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import '/shared/settings/controls/settings_dropdown_menu.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {getTemplate} from './keyboard.html.js';

/**
 * Modifier key IDs corresponding to the ModifierKey enumerators in
 * /ui/base/ime/ash/ime_keyboard.h.
 */
enum ModifierKey {
  SEARCH_KEY = 0,
  CONTROL_KEY = 1,
  ALT_KEY = 2,
  VOID_KEY = 3,  // Represents a disabled key.
  CAPS_LOCK_KEY = 4,
  ESCAPE_KEY = 5,
  BACKSPACE_KEY = 6,
  ASSISTANT_KEY = 7,
}

const SettingsKeyboardElementBase =
    DeepLinkingMixin(RouteObserverMixin(WebUiListenerMixin(PolymerElement)));

class SettingsKeyboardElement extends SettingsKeyboardElementBase {
  static get is() {
    return 'settings-keyboard';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      focusConfig: {
        type: Object,
        observer: 'onFocusConfigChange_',
      },

      /** Whether to show Caps Lock options. */
      showCapsLock_: Boolean,

      /**
       * Whether this device has a ChromeOS launcher key. Applies only to
       * ChromeOS keyboards, internal or external.
       */
      hasLauncherKey_: Boolean,

      /** Whether this device has an Assistant key on keyboard. */
      hasAssistantKey_: Boolean,

      /**
       * Whether to show a remapping option for external keyboard's Meta key
       * (Search/Windows keys). This is true only when there's an external
       * keyboard connected that is a non-Apple keyboard.
       */
      showExternalMetaKey_: Boolean,

      /**
       * Whether to show a remapping option for the Command key. This is true
       * when one of the connected keyboards is an Apple keyboard.
       */
      showAppleCommandKey_: Boolean,

      /** Menu items for key mapping. */
      keyMapTargets_: Object,

      /**
       * Auto-repeat delays (in ms) for the corresponding slider values, from
       * long to short. The values were chosen to provide a large range while
       * giving several options near the defaults.
       */
      autoRepeatDelays_: {
        type: Array,
        value: [2000, 1500, 1000, 500, 300, 200, 150],
        readOnly: true,
      },

      /**
       * Auto-repeat intervals (in ms) for the corresponding slider values, from
       * long to short. The slider itself is labeled "rate", the inverse of
       * interval, and goes from slow (long interval) to fast (short interval).
       */
      autoRepeatIntervals_: {
        type: Array,
        value: [2000, 1000, 500, 300, 200, 100, 50, 30, 20],
        readOnly: true,
      },

      /**
       * Whether the setting for long press diacritics should be shown
       */
      shouldShowDiacriticSetting_: Boolean,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kKeyboardFunctionKeys,
          Setting.kKeyboardAutoRepeat,
          Setting.kKeyboardShortcuts,
          Setting.kShowDiacritic,
        ]),
      },
    };
  }

  focusConfig: FocusConfig;
  private browserProxy_: DevicePageBrowserProxy;
  private hasAssistantKey_: boolean;
  private hasLauncherKey_: boolean;
  private keyMapTargets_: DropdownMenuOptionList;
  private showAppleCommandKey_: boolean;
  private showCapsLock_: boolean;
  private showExternalMetaKey_: boolean;
  private shouldShowDiacriticSetting_ =
      loadTimeData.getBoolean('allowDiacriticsOnPhysicalKeyboardLongpress');

  constructor() {
    super();

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'show-keys-changed', this.onShowKeysChange_.bind(this));
    this.browserProxy_.initializeKeyboard();
    this.setUpKeyMapTargets_();
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.KEYBOARD) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Initializes the dropdown menu options for remapping keys.
   */
  private setUpKeyMapTargets_() {
    // Ordering is according to UX, but values match ModifierKey.
    this.keyMapTargets_ = [
      {
        value: ModifierKey.SEARCH_KEY,
        name: loadTimeData.getString('keyboardKeySearch'),
      },
      {
        value: ModifierKey.CONTROL_KEY,
        name: loadTimeData.getString('keyboardKeyCtrl'),
      },
      {
        value: ModifierKey.ALT_KEY,
        name: loadTimeData.getString('keyboardKeyAlt'),
      },
      {
        value: ModifierKey.CAPS_LOCK_KEY,
        name: loadTimeData.getString('keyboardKeyCapsLock'),
      },
      {
        value: ModifierKey.ESCAPE_KEY,
        name: loadTimeData.getString('keyboardKeyEscape'),
      },
      {
        value: ModifierKey.BACKSPACE_KEY,
        name: loadTimeData.getString('keyboardKeyBackspace'),
      },
      {
        value: ModifierKey.ASSISTANT_KEY,
        name: loadTimeData.getString('keyboardKeyAssistant'),
      },
      {
        value: ModifierKey.VOID_KEY,
        name: loadTimeData.getString('keyboardKeyDisabled'),
      },
    ];
  }

  private onFocusConfigChange_() {
    this.focusConfig.set(routes.OS_LANGUAGES_INPUT.path, () => {
      afterNextRender(this, () => {
        const showLanguagesInputEl =
            castExists(this.shadowRoot!.getElementById('showLanguagesInput'));
        focusWithoutInk(showLanguagesInputEl);
      });
    });
  }

  /**
   * Handler for updating which keys to show.
   */
  private onShowKeysChange_(keyboardParams: {[key: string]: boolean}) {
    this.hasLauncherKey_ = keyboardParams['hasLauncherKey'];
    this.hasAssistantKey_ = keyboardParams['hasAssistantKey'];
    this.showCapsLock_ = keyboardParams['showCapsLock'];
    this.showExternalMetaKey_ = keyboardParams['showExternalMetaKey'];
    this.showAppleCommandKey_ = keyboardParams['showAppleCommandKey'];
  }

  private onShowKeyboardShortcutViewerClick_() {
    this.browserProxy_.showKeyboardShortcutViewer();
  }

  private onShowInputSettingsClick_() {
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT,
        /*dynamicParams=*/ undefined, /*removeSearch=*/ true);
  }

  private getExternalMetaKeyLabel_(hasLauncherKey: boolean): string {
    return loadTimeData.getString(
        hasLauncherKey ? 'keyboardKeyExternalMeta' : 'keyboardKeyMeta');
  }

  private getExternalCommandKeyLabel_(hasLauncherKey: boolean): string {
    return loadTimeData.getString(
        hasLauncherKey ? 'keyboardKeyExternalCommand' : 'keyboardKeyCommand');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-keyboard': SettingsKeyboardElement;
  }
}

customElements.define(SettingsKeyboardElement.is, SettingsKeyboardElement);
