// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-power' is the settings subpage for power settings.
 */

import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, PowerManagementSettings, PowerSource} from './device_page_browser_proxy.js';
import {getTemplate} from './power.html.js';

interface IdleOption {
  value: IdleBehavior;
  name: string;
  selected: boolean;
}

interface SettingsPowerElement {
  $: {
    adaptiveChargingToggle: SettingsToggleButtonElement,
    lidClosedToggle: SettingsToggleButtonElement,
    powerSource: HTMLSelectElement,
  };
}

const SettingsPowerElementBase = DeepLinkingMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

class SettingsPowerElement extends SettingsPowerElementBase {
  static get is() {
    return 'settings-power';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * ID of the selected power source, or ''.
       */
      selectedPowerSourceId_: String,

      batteryStatus_: Object,

      /**
       * Whether a low-power (USB) charger is being used.
       */
      lowPowerCharger_: Boolean,

      /**
       *  Whether the AC idle behavior is managed by policy.
       */
      acIdleManaged_: Boolean,

      /**
       * Whether the battery idle behavior is managed by policy.
       */
      batteryIdleManaged_: Boolean,

      /**
       * Text for label describing the lid-closed behavior.
       */
      lidClosedLabel_: String,

      /**
       * Whether the system possesses a lid.
       */
      hasLid_: Boolean,

      /**
       * List of available dual-role power sources.
       */
      powerSources_: Array,

      powerSourceLabel_: {
        type: String,
        computed:
            'computePowerSourceLabel_(powerSources_, batteryStatus_.calculating)',
      },

      showPowerSourceDropdown_: {
        type: Boolean,
        computed: 'computeShowPowerSourceDropdown_(powerSources_)',
        value: false,
      },

      /**
       * The name of the dedicated charging device being used, if present.
       */
      powerSourceName_: {
        type: String,
        computed: 'computePowerSourceName_(powerSources_, lowPowerCharger_)',
      },

      acIdleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      batteryIdleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      shouldAcIdleSelectBeDisabled_: {
        type: Boolean,
        computed: 'hasSingleOption_(acIdleOptions_)',
      },

      shouldBatteryIdleSelectBeDisabled_: {
        type: Boolean,
        computed: 'hasSingleOption_(batteryIdleOptions_)',
      },

      adaptiveChargingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAdaptiveChargingEnabled');
        },
      },

      /** Whether adaptive charging is managed by policy. */
      adaptiveChargingManaged_: Boolean,

      lidClosedPref_: {
        type: Object,
        value() {
          return {};
        },
      },

      adaptiveChargingPref_: {
        type: Object,
        value() {
          return {};
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kPowerIdleBehaviorWhileCharging,
          Setting.kPowerSource,
          Setting.kSleepWhenLaptopLidClosed,
          Setting.kPowerIdleBehaviorWhileOnBattery,
          Setting.kAdaptiveCharging,
        ]),
      },

    };
  }

  private acIdleManaged_: boolean;
  private acIdleOptions_: IdleOption[];
  private adaptiveChargingEnabled_: boolean;
  private adaptiveChargingManaged_: boolean;
  private adaptiveChargingPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private batteryIdleManaged_: boolean;
  private batteryIdleOptions_: IdleOption[];
  private batteryStatus_: BatteryStatus|undefined;
  private browserProxy_: DevicePageBrowserProxy;
  private hasLid_: boolean;
  private lidClosedLabel_: string;
  private lidClosedPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private lowPowerCharger_: boolean;
  private powerSources_: PowerSource[]|undefined;
  private selectedPowerSourceId_: string;

  constructor() {
    super();

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
    this.addWebUiListener(
        'power-sources-changed', this.powerSourcesChanged_.bind(this));
    this.browserProxy_.updatePowerStatus();

    this.addWebUiListener(
        'power-management-settings-changed',
        this.powerManagementSettingsChanged_.bind(this));
    this.browserProxy_.requestPowerManagementSettings();
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId === Setting.kPowerSource && this.$.powerSource.hidden) {
      // If there is only 1 power source, there is no dropdown to focus.
      // Stop the deep link attempt in this case.
      return false;
    }

    // Continue with deep link attempt.
    return true;
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.POWER) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return The primary label for the power source row.
   */
  private computePowerSourceLabel_(
      powerSources: PowerSource[]|undefined, calculating: boolean): string {
    return this.i18n(
        calculating                             ? 'calculatingPower' :
            powerSources && powerSources.length ? 'powerSourceLabel' :
                                                  'powerSourceBattery');
  }

  /**
   * @return True if at least one power source is attached and all of
   *     them are dual-role (no dedicated chargers).
   */
  private computeShowPowerSourceDropdown_(powerSources: PowerSource[]):
      boolean {
    return powerSources.length > 0 &&
        powerSources.every((source) => !source.is_dedicated_charger);
  }

  /**
   * @return Description of the power source.
   */
  private computePowerSourceName_(
      powerSources: PowerSource[], lowPowerCharger: boolean): string {
    if (lowPowerCharger) {
      return this.i18n('powerSourceLowPowerCharger');
    }
    if (powerSources.length) {
      return this.i18n('powerSourceAcAdapter');
    }
    return '';
  }

  private onPowerSourceChange_(): void {
    this.browserProxy_.setPowerSource(this.$.powerSource.value);
  }

  /**
   * Used to disable Battery/AC idle select dropdowns.
   */
  private hasSingleOption_(idleOptions: string[]): boolean {
    return idleOptions.length === 1;
  }

  private onAcIdleSelectChange_(event: Event): void {
    const behavior: IdleBehavior =
        parseInt((event.target as HTMLSelectElement).value, 10);
    this.browserProxy_.setIdleBehavior(behavior, /* whenOnAc */ true);
    recordSettingChange();
  }

  private onBatteryIdleSelectChange_(event: Event): void {
    const behavior: IdleBehavior =
        parseInt((event.target as HTMLSelectElement).value, 10);
    this.browserProxy_.setIdleBehavior(behavior, /* whenOnAc */ false);
    recordSettingChange();
  }

  private onLidClosedToggleChange_(): void {
    // Other behaviors are only displayed when the setting is controlled, in
    // which case the toggle can't be changed by the user.
    this.browserProxy_.setLidClosedBehavior(
        this.$.lidClosedToggle.checked ? LidClosedBehavior.SUSPEND :
                                         LidClosedBehavior.DO_NOTHING);
    recordSettingChange();
  }

  private onAdaptiveChargingToggleChange_(): void {
    const enabled = this.$.adaptiveChargingToggle.checked;
    this.browserProxy_.setAdaptiveCharging(enabled);
    recordSettingChange(Setting.kAdaptiveCharging, {boolValue: enabled});
  }

  /**
   * @param sources External power sources.
   * @param selectedId The ID of the currently used power source.
   * @param lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   */
  private powerSourcesChanged_(
      sources: PowerSource[], selectedId: string,
      lowPowerCharger: boolean): void {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  }

  /**
   * @param behavior Current behavior.
   * @param isControlled Whether the underlying pref is controlled.
   */
  private updateLidClosedLabelAndPref_(
      behavior: LidClosedBehavior, isControlled: boolean): void {
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      // Most behaviors get a dedicated label and appear as checked.
      value: true,
    };

    switch (behavior) {
      case LidClosedBehavior.SUSPEND:
      case LidClosedBehavior.DO_NOTHING:
        // "Suspend" and "do nothing" share the "sleep" label and communicate
        // their state via the toggle state.
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSleepLabel');
        pref.value = behavior === LidClosedBehavior.SUSPEND;
        break;
      case LidClosedBehavior.STOP_SESSION:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSignOutLabel');
        break;
      case LidClosedBehavior.SHUT_DOWN:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidShutDownLabel');
        break;
    }

    if (isControlled) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    this.lidClosedPref_ = pref;
  }

  /**
   * @return Idle option object that maps to idleBehavior.
   */
  private getIdleOption_(
      idleBehavior: IdleBehavior, currIdleBehavior: IdleBehavior):
      {value: IdleBehavior, name: string, selected: boolean} {
    const selected = idleBehavior === currIdleBehavior;
    switch (idleBehavior) {
      case IdleBehavior.DISPLAY_OFF_SLEEP:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOffSleep'),
          selected: selected,
        };
      case IdleBehavior.DISPLAY_OFF:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOff'),
          selected: selected,
        };
      case IdleBehavior.DISPLAY_ON:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOn'),
          selected: selected,
        };
      case IdleBehavior.SHUT_DOWN:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayShutDown'),
          selected: selected,
        };
      case IdleBehavior.STOP_SESSION:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayStopSession'),
          selected: selected,
        };
      default:
        assertNotReached('Unknown IdleBehavior type');
    }
  }

  private updateIdleOptions_(
      acIdleBehaviors: IdleBehavior[], batteryIdleBehaviors: IdleBehavior[],
      currAcIdleBehavior: IdleBehavior, currBatteryIdleBehavior: IdleBehavior) {
    this.acIdleOptions_ = acIdleBehaviors.map((idleBehavior) => {
      return this.getIdleOption_(idleBehavior, currAcIdleBehavior);
    });

    this.batteryIdleOptions_ = batteryIdleBehaviors.map((idleBehavior) => {
      return this.getIdleOption_(idleBehavior, currBatteryIdleBehavior);
    });
  }

  /**
   * @param powerManagementSettings Current power management settings.
   */
  private powerManagementSettingsChanged_(powerManagementSettings:
                                              PowerManagementSettings) {
    this.updateIdleOptions_(
        powerManagementSettings.possibleAcIdleBehaviors || [],
        powerManagementSettings.possibleBatteryIdleBehaviors || [],
        powerManagementSettings.currentAcIdleBehavior,
        powerManagementSettings.currentBatteryIdleBehavior);
    this.acIdleManaged_ = powerManagementSettings.acIdleManaged;
    this.batteryIdleManaged_ = powerManagementSettings.batteryIdleManaged;
    this.hasLid_ = powerManagementSettings.hasLid;
    this.updateLidClosedLabelAndPref_(
        powerManagementSettings.lidClosedBehavior,
        powerManagementSettings.lidClosedControlled);
    this.adaptiveChargingManaged_ =
        powerManagementSettings.adaptiveChargingManaged;
    // Use an atomic assign to trigger UI change.
    const adaptiveChargingPref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: powerManagementSettings.adaptiveCharging,
    };
    if (this.adaptiveChargingManaged_) {
      adaptiveChargingPref.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      adaptiveChargingPref.controlledBy =
          chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    }
    this.adaptiveChargingPref_ = adaptiveChargingPref;
  }

  /**
   * Returns the row class for the given settings row
   * @param batteryPresent if battery is present
   * @param element the name of the row being queried
   * @return the class for the given row
   */
  private getClassForRow_(batteryPresent: boolean, element: string): string {
    let c = 'cr-row';

    switch (element) {
      case 'adaptiveCharging':
        if (!batteryPresent) {
          c += ' first';
        }
        break;
      case 'idle':
        if (!batteryPresent && !this.adaptiveChargingEnabled_) {
          c += ' first';
        }
        break;
    }

    return c;
  }

  private isEqual_(lhs: string, rhs: string): boolean {
    return lhs === rhs;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-power': SettingsPowerElement;
  }
}

customElements.define(SettingsPowerElement.is, SettingsPowerElement);
