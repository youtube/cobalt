// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {ActionChoice, Button, ButtonPressObserverInterface, GraphicsTablet, GraphicsTabletObserverInterface, GraphicsTabletSettings, InputDeviceSettingsProviderInterface, Keyboard, KeyboardObserverInterface, KeyboardSettings, MetaKey, ModifierKey, Mouse, MouseObserverInterface, MouseSettings, PointingStick, PointingStickObserverInterface, PointingStickSettings, SixPackShortcutModifier, Stylus, StylusObserverInterface, Touchpad, TouchpadObserverInterface, TouchpadSettings} from './input_device_settings_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FakePerDeviceKeyboardProvider mojo
 * interface.
 */

interface InputDeviceSettingsType {
  fakeKeyboards: Keyboard[];
  fakeTouchpads: Touchpad[];
  fakeMice: Mouse[];
  fakePointingSticks: PointingStick[];
  fakeStyluses: Stylus[];
  fakeGraphicsTablets: GraphicsTablet[];
  fakeMouseButtonActions: {options: ActionChoice[]};
  fakeGraphicsTabletButtonActions: {options: ActionChoice[]};
}

class FakeMethodState {
  private result = undefined;

  resolveMethod(): Promise<any> {
    const promise = new Promise((resolve) => {
      resolve(this.result);
    });
    return promise;
  }

  getResult(): any {
    return this.result;
  }

  setResult(result: any): void {
    this.result = result;
  }
}

/**
 * Manages a map of fake async methods, their resolvers and the fake
 * return value they will produce.
 */
export class FakeMethodResolver {
  private methodMap: Map<string, FakeMethodState> = new Map();

  register(methodName: string): void {
    this.methodMap.set(methodName, new FakeMethodState());
  }

  getResult<K extends keyof InputDeviceSettingsType, T>(methodName: K):
      InputDeviceSettingsType[K] extends T? InputDeviceSettingsType[K]: never {
    return this.getState(methodName).getResult();
  }

  setResult<K extends keyof InputDeviceSettingsType, T>(
      methodName: K, result: InputDeviceSettingsType[K] extends T?
      InputDeviceSettingsType[K]: never): void {
    this.getState(methodName).setResult(result);
  }

  resolveMethod<T extends keyof InputDeviceSettingsType>(methodName: T):
      Promise<InputDeviceSettingsType[T]> {
    return this.getState(methodName).resolveMethod();
  }

  getState(methodName: string): FakeMethodState {
    const state = this.methodMap.get(methodName);
    assert(!!state, `Method '${methodName}' not found.`);
    return state;
  }
}

export class FakeInputDeviceSettingsProvider implements
    InputDeviceSettingsProviderInterface {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private keyboardObservers: KeyboardObserverInterface[] = [];
  private pointingStickObservers: PointingStickObserverInterface[] = [];
  private mouseObservers: MouseObserverInterface[] = [];
  private touchpadObservers: TouchpadObserverInterface[] = [];
  private stylusObservers: StylusObserverInterface[] = [];
  private graphicsTabletObservers: GraphicsTabletObserverInterface[] = [];
  private buttonPressObservers: ButtonPressObserverInterface[] = [];
  private observedIds: number[] = [];
  private callCounts_ = {
    setGraphicsTabletSettings: 0,
    setMouseSettings: 0,
  };

  constructor() {
    // Setup method resolvers.
    this.methods.register('fakeKeyboards');
    this.methods.register('fakeTouchpads');
    this.methods.register('fakeMice');
    this.methods.register('fakePointingSticks');
    this.methods.register('fakeStyluses');
    this.methods.register('fakeGraphicsTablets');
    this.methods.register('fakeMouseButtonActions');
    this.methods.register('fakeGraphicsTabletButtonActions');
  }

  setFakeKeyboards(keyboards: Keyboard[]): void {
    this.methods.setResult('fakeKeyboards', keyboards);
    this.notifyKeboardListUpdated();
  }

  getConnectedKeyboardSettings(): Promise<Keyboard[]> {
    return this.methods.resolveMethod('fakeKeyboards');
  }

  setFakeTouchpads(touchpads: Touchpad[]): void {
    this.methods.setResult('fakeTouchpads', touchpads);
    this.notifyTouchpadListUpdated();
  }

  getConnectedTouchpadSettings(): Promise<Touchpad[]> {
    return this.methods.resolveMethod('fakeTouchpads');
  }

  setFakeMice(mice: Mouse[]): void {
    this.methods.setResult('fakeMice', mice);
    this.notifyMouseListUpdated();
  }

  getConnectedMouseSettings(): Promise<Mouse[]> {
    return this.methods.resolveMethod('fakeMice');
  }

  setFakePointingSticks(pointingSticks: PointingStick[]): void {
    this.methods.setResult('fakePointingSticks', pointingSticks);
    this.notifyPointingStickListUpdated();
  }

  getConnectedPointingStickSettings(): Promise<PointingStick[]> {
    return this.methods.resolveMethod('fakePointingSticks');
  }

  setFakeStyluses(styluses: Stylus[]): void {
    this.methods.setResult('fakeStyluses', styluses);
  }

  getConnectedStylusSettings(): Promise<Stylus[]> {
    return this.methods.resolveMethod('fakeStyluses');
  }

  setFakeGraphicsTablets(graphicsTablets: GraphicsTablet[]): void {
    this.methods.setResult('fakeGraphicsTablets', graphicsTablets);
    this.notifyGraphicsTabletListUpdated();
  }

  getConnectedGraphicsTabletSettings(): Promise<GraphicsTablet[]> {
    return this.methods.resolveMethod('fakeGraphicsTablets');
  }

  restoreDefaultKeyboardRemappings(id: number): void {
    const keyboards = this.methods.getResult('fakeKeyboards');
    for (const keyboard of keyboards) {
      if (keyboard.id === id) {
        keyboard.settings.modifierRemappings =
            keyboard.metaKey === MetaKey.kCommand ? {
              [ModifierKey.kControl]: ModifierKey.kMeta,
              [ModifierKey.kMeta]: ModifierKey.kControl,
            } :
                                                    {};
        keyboard.settings.sixPackKeyRemappings = {
          pageDown: SixPackShortcutModifier.kSearch,
          pageUp: SixPackShortcutModifier.kSearch,
          del: SixPackShortcutModifier.kSearch,
          insert: SixPackShortcutModifier.kSearch,
          home: SixPackShortcutModifier.kSearch,
          end: SixPackShortcutModifier.kSearch,
        };
      }
    }
    this.methods.setResult('fakeKeyboards', keyboards);
    this.notifyKeboardListUpdated();
  }

  setKeyboardSettings(id: number, settings: KeyboardSettings): void {
    const keyboards = this.methods.getResult('fakeKeyboards');
    for (const keyboard of keyboards) {
      if (keyboard.id === id) {
        keyboard.settings = settings;
      }
    }
    this.methods.setResult('fakeKeyboards', keyboards);
  }

  setMouseSettings(id: number, settings: MouseSettings): void {
    const mice = this.methods.getResult('fakeMice');
    for (const mouse of mice) {
      if (mouse.id === id) {
        mouse.settings = settings;
      }
    }
    this.methods.setResult('fakeMice', mice);
    this.notifyMouseListUpdated();
    this.callCounts_.setMouseSettings++;
  }

  getSetMouseSettingsCallCount(): number {
    return this.callCounts_.setMouseSettings;
  }

  setTouchpadSettings(id: number, settings: TouchpadSettings): void {
    const touchpads = this.methods.getResult('fakeTouchpads');
    for (const touchpad of touchpads) {
      if (touchpad.id === id) {
        touchpad.settings = settings;
      }
    }
    this.methods.setResult('fakeTouchpads', touchpads);
  }

  setPointingStickSettings(id: number, settings: PointingStickSettings): void {
    const pointingSticks = this.methods.getResult('fakePointingSticks');
    for (const pointingStick of pointingSticks) {
      if (pointingStick.id === id) {
        pointingStick.settings = settings;
      }
    }
    this.methods.setResult('fakePointingSticks', pointingSticks);
  }

  setGraphicsTabletSettings(id: number, settings: GraphicsTabletSettings):
      void {
    const graphicsTablets = this.methods.getResult('fakeGraphicsTablets');
    for (const graphicsTablet of graphicsTablets) {
      if (graphicsTablet.id === id) {
        graphicsTablet.settings = settings;
      }
    }
    this.methods.setResult('fakeGraphicsTablets', graphicsTablets);
    this.notifyGraphicsTabletListUpdated();
    this.callCounts_.setGraphicsTabletSettings++;
  }

  getSetGraphicsTabletSettingsCallCount(): number {
    return this.callCounts_.setGraphicsTabletSettings;
  }

  notifyKeboardListUpdated(): void {
    const keyboards = this.methods.getResult('fakeKeyboards');
    // Make a deep copy to notify the functions observing keyboard settings.
    const keyboardsClone =
        !keyboards ? keyboards : JSON.parse(JSON.stringify(keyboards));
    for (const observer of this.keyboardObservers) {
      observer.onKeyboardListUpdated(keyboardsClone);
    }
  }

  notifyTouchpadListUpdated(): void {
    const touchpads = this.methods.getResult('fakeTouchpads');
    for (const observer of this.touchpadObservers) {
      observer.onTouchpadListUpdated(touchpads);
    }
  }

  notifyMouseListUpdated(): void {
    const mice = this.methods.getResult('fakeMice');
    for (const observer of this.mouseObservers) {
      observer.onMouseListUpdated(mice);
    }
  }

  notifyPointingStickListUpdated(): void {
    const pointingSticks = this.methods.getResult('fakePointingSticks');
    for (const observer of this.pointingStickObservers) {
      observer.onPointingStickListUpdated(pointingSticks);
    }
  }

  notifyStylusListUpdated(): void {
    const styluses = this.methods.getResult('fakeStyluses');
    for (const observer of this.stylusObservers) {
      observer.onStylusListUpdated(styluses);
    }
  }

  notifyGraphicsTabletListUpdated(): void {
    const graphicsTablets = this.methods.getResult('fakeGraphicsTablets');
    for (const observer of this.graphicsTabletObservers) {
      observer.onGraphicsTabletListUpdated(graphicsTablets);
    }
  }

  observeKeyboardSettings(observer: KeyboardObserverInterface): void {
    this.keyboardObservers.push(observer);
    this.notifyKeboardListUpdated();
  }

  observeTouchpadSettings(observer: TouchpadObserverInterface): void {
    this.touchpadObservers.push(observer);
    this.notifyTouchpadListUpdated();
  }

  observeMouseSettings(observer: MouseObserverInterface): void {
    this.mouseObservers.push(observer);
    this.notifyMouseListUpdated();
  }

  observePointingStickSettings(observer: PointingStickObserverInterface): void {
    this.pointingStickObservers.push(observer);
    this.notifyPointingStickListUpdated();
  }

  observeStylusSettings(observer: StylusObserverInterface): void {
    this.stylusObservers.push(observer);
    this.notifyStylusListUpdated();
  }

  observeGraphicsTabletSettings(observer: GraphicsTabletObserverInterface):
      void {
    this.graphicsTabletObservers.push(observer);
    this.notifyGraphicsTabletListUpdated();
  }

  observeButtonPresses(observer: ButtonPressObserverInterface): void {
    this.buttonPressObservers.push(observer);
  }

  getActionsForMouseButtonCustomization(): Promise<{options: ActionChoice[]}> {
    return this.methods.resolveMethod('fakeMouseButtonActions');
  }

  setFakeActionsForMouseButtonCustomization(actionChoices: ActionChoice[]):
      void {
    this.methods.setResult('fakeMouseButtonActions', {options: actionChoices});
  }

  getActionsForGraphicsTabletButtonCustomization():
      Promise<{options: ActionChoice[]}> {
    return this.methods.resolveMethod('fakeGraphicsTabletButtonActions');
  }

  setFakeActionsForGraphicsTabletButtonCustomization(actionChoices:
                                                         ActionChoice[]): void {
    this.methods.setResult(
        'fakeGraphicsTabletButtonActions', {options: actionChoices});
  }

  startObserving(id: number): void {
    if (this.observedIds.includes(id)) {
      return;
    }
    this.observedIds.push(id);
  }

  stopObserving(): void {
    this.observedIds = [];
  }

  getObservedDevices(): number[] {
    return this.observedIds;
  }

  sendButtonPress(button: Button): void {
    for (const observer of this.buttonPressObservers) {
      observer.onButtonPressed(button);
    }
  }
}
