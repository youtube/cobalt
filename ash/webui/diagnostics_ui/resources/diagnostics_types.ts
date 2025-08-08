// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import {KeyboardInfo} from './input.mojom-webui.js';
import {TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

export interface NetworkGuidInfo {
  networkGuids: string[];
  activeGuid: string;
}

// Radio band related to channel frequency.
export enum ChannelBand {
  UNKNOWN,
  /** 5Ghz radio band. */
  FIVE_GHZ,
  /** 2.4Ghz radio band. */
  TWO_DOT_FOUR_GHZ,
}

// Struct for holding data related to WiFi network channel.
export interface ChannelProperties {
  channel: number;
  band: ChannelBand;
}

export interface RoutineProperties {
  routine: RoutineType;
  blocking: boolean;
}

export interface TroubleshootingInfo {
  header: string;
  linkText: string;
  url: string;
}

/**
 * Type alias for ash::diagnostics::metrics::NavigationView to support message
 * handler logic and metric recording. Enum values need to be kept in sync with
 * "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h".
 */
export enum NavigationView {
  SYSTEM,
  CONNECTIVITY,
  INPUT,
  MAX_VALUE,
}

// Type alias for the the response from InputDataProvider.GetConnectedDevices.
export interface GetConnectedDevicesResponse {
  keyboards: KeyboardInfo[];
  touchDevices: TouchDeviceInfo[];
}
