// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorState, AcceleratorType, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from 'chrome://shortcut-customization/js/shortcut_types.js';


export function createStandardAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
        },
      },
    },
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kDefault,
  };
}

export function createTextAcceleratorInfo(
    parts: TextAcceleratorPart[], locked = false): TextAcceleratorInfo {
  return {
    layoutProperties: {
      textAccelerator: {
        parts,
      },
    },
    locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kDefault,
  };
}

export function createUserAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
        },
      },
    },
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kUser,
  };
}
