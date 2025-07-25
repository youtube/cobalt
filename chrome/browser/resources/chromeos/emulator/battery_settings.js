// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './icons.js';
import './shared_styles.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'battery-settings',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** The system's battery percentage. */
    batteryPercent: Number,

    /**
     * A string representing a value in the
     * PowerSupplyProperties_BatteryState enumeration.
     */
    batteryState: {
      type: String,
      observer: 'batteryStateChanged',
    },

    /**
     * An array representing the battery state options.
     * The names are ordered based on the
     * PowerSupplyProperties_BatteryState enumeration. These values must be
     * in sync.
     */
    batteryStateOptions: {
      type: Array,
      value() {
        return ['Full', 'Charging', 'Discharging', 'Not Present'];
      },
    },

    /**
     * Example charging devices that can be connected. Chargers are split
     * between dedicated chargers (which will always provide power if no
     * higher-power dedicated charger is connected) and dual-role USB chargers
     * (which only provide power if configured as a source and no dedicated
     * charger is connected).
     */
    powerSourceOptions: {
      type: Array,
      value() {
        return [
          {
            id: '0',
            name: 'AC Charger 1',
            type: 'DedicatedCharger',
            port: 0,
            connected: false,
            power: 'high',
          },
          {
            id: '1',
            name: 'AC Charger 2',
            type: 'DedicatedCharger',
            port: 0,
            connected: false,
            power: 'high',
          },
          {
            id: '2',
            name: 'USB Charger 1',
            type: 'DedicatedCharger',
            port: 0,
            connected: false,
            power: 'low',
            variablePower: true,
          },
          {
            id: '3',
            name: 'USB Charger 2',
            type: 'DedicatedCharger',
            port: 0,
            connected: false,
            power: 'low',
            variablePower: true,
          },
          {
            id: '4',
            name: 'Dual-role USB 1',
            type: 'DualRoleUSB',
            port: 0,
            connected: false,
            power: 'low',
          },
          {
            id: '5',
            name: 'Dual-role USB 2',
            type: 'DualRoleUSB',
            port: 1,
            connected: false,
            power: 'low',
          },
          {
            id: '6',
            name: 'Dual-role USB 3',
            type: 'DualRoleUSB',
            port: 2,
            connected: false,
            power: 'low',
          },
          {
            id: '7',
            name: 'Dual-role USB 4',
            type: 'DualRoleUSB',
            port: 3,
            connected: false,
            power: 'low',
          },
        ];
      },
    },

    /** The ID of the current power source, or the empty string. */
    selectedPowerSourceId: String,

    /** A number representing the time left until the battery is discharged. */
    timeUntilEmpty: Number,

    /** A number representing the time left until the battery is at 100%. */
    timeUntilFull: Number,
  },

  observers: [
    'powerSourcesChanged(powerSourceOptions.*)',
  ],

  ready() {
    this.addWebUIListener(
        'power-properties-updated', this.onPowerPropertiesUpdated_.bind(this));
    chrome.send('requestPowerInfo');
  },

  onBatteryPercentChange(e) {
    this.percent = parseInt(e.target.value, 10);
    if (!isNaN(this.percent)) {
      chrome.send('updateBatteryPercent', [this.percent]);
    }
  },

  /**
   * @param {!{model: {item: {id: string}}}} e
   * @private
   */
  onSetAsSourceClick_(e) {
    chrome.send('updatePowerSourceId', [e.model.item.id]);
  },

  batteryStateChanged(batteryState) {
    // Find the index of the selected battery state.
    const index = this.batteryStateOptions.indexOf(batteryState);
    if (index < 0) {
      return;
    }
    chrome.send('updateBatteryState', [index]);
  },

  powerSourcesChanged() {
    const connectedPowerSources =
        this.powerSourceOptions.filter(function(source) {
          return source.connected;
        });
    chrome.send('updatePowerSources', [connectedPowerSources]);
  },

  onTimeUntilEmptyChange(e) {
    this.timeUntilEmpty = parseInt(e.target.value, 10);
    if (!isNaN(this.timeUntilEmpty)) {
      chrome.send('updateTimeToEmpty', [this.timeUntilEmpty]);
    }
  },

  onTimeUntilFullChange(e) {
    this.timeUntilFull = parseInt(e.target.value, 10);
    if (!isNaN(this.timeUntilFull)) {
      chrome.send('updateTimeToFull', [this.timeUntilFull]);
    }
  },

  onPowerChanged(e) {
    e.model.set('item.power', e.target.value);
  },

  /**
   * @param {{
   *   battery_percent: number,
   *   battery_state: number,
   *   battery_time_to_empty_sec: number,
   *   battery_time_to_full_sec: number,
   *   external_power_source_id: string,
   * }} properties
   * @private
   */
  onPowerPropertiesUpdated_(properties) {
    this.batteryPercent = properties.battery_percent;
    this.batteryState = this.batteryStateOptions[properties.battery_state];
    this.timeUntilEmpty = properties.battery_time_to_empty_sec;
    this.timeUntilFull = properties.battery_time_to_full_sec;
    this.selectedPowerSourceId = properties.external_power_source_id;
  },

  isBatteryPresent() {
    return this.batteryState != 'Not Present';
  },

  isDualRole(source) {
    return source.type == 'DualRoleUSB';
  },

  /**
   * @param {!{id: string}} source
   * @return {string}
   * @private
   */
  cssClassForSetAsSource_(source) {
    return source.id == this.selectedPowerSourceId ? '' : 'action-button';
  },

  canAmpsChange(type) {
    return type == 'USB';
  },

  canBecomeSource(source, selectedId, powerSourceOptionsChange) {
    if (!source.connected || !this.isDualRole(source)) {
      return false;
    }
    return !this.powerSourceOptions.some(function(source) {
      return source.connected && source.type == 'DedicatedCharger';
    });
  },
});
