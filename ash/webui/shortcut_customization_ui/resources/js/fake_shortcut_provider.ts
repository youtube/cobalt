// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {AcceleratorResultData, AcceleratorsUpdatedObserverRemote} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {Accelerator, AcceleratorConfig, AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';


/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

// Method names.
const ON_ACCELERATORS_UPDATED_METHOD_NAME =
    'AcceleratorsUpdatedObserver_OnAcceleratorsUpdated';

export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods: FakeMethodResolver;
  private observables: FakeObservables = new FakeObservables();
  private acceleratorsUpdatedRemote: AcceleratorsUpdatedObserverRemote|null =
      null;
  private acceleratorsUpdatedPromise: Promise<void>|null = null;
  private restoreDefaultCallCount: number = 0;
  private preventProcessingAcceleratorsCallCount: number = 0;

  constructor() {
    this.methods = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods.register('getAccelerators');
    this.methods.register('getAcceleratorLayoutInfos');
    this.methods.register('isMutable');
    this.methods.register('hasLauncherButton');
    this.methods.register('addAccelerator');
    this.methods.register('replaceAccelerator');
    this.methods.register('removeAccelerator');
    this.methods.register('restoreDefault');
    this.methods.register('restoreAllDefaults');
    this.methods.register('addObserver');
    this.methods.register('preventProcessingAccelerators');
    this.registerObservables();
  }

  registerObservables(): void {
    this.observables.register(ON_ACCELERATORS_UPDATED_METHOD_NAME);
  }

  // Disable all observers and reset provider to initial state.
  reset(): void {
    this.restoreDefaultCallCount = 0;
    this.preventProcessingAcceleratorsCallCount = 0;
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  triggerOnAcceleratorUpdated(): void {
    this.observables.trigger(ON_ACCELERATORS_UPDATED_METHOD_NAME);
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.methods.resolveMethod('getAcceleratorLayoutInfos');
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.methods.resolveMethod('getAccelerators');
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    this.methods.setResult(
        'isMutable', {isMutable: source !== AcceleratorSource.kBrowser});
    return this.methods.resolveMethod('isMutable');
  }

  hasLauncherButton(): Promise<{hasLauncherButton: boolean}> {
    return this.methods.resolveMethod('hasLauncherButton');
  }

  addObserver(observer: AcceleratorsUpdatedObserverRemote): void {
    this.acceleratorsUpdatedPromise = this.observe(
        ON_ACCELERATORS_UPDATED_METHOD_NAME, (config: AcceleratorConfig) => {
          observer.onAcceleratorsUpdated(config);
        });
  }

  getAcceleratorsUpdatedPromiseForTesting(): Promise<void> {
    assert(this.acceleratorsUpdatedPromise);
    return this.acceleratorsUpdatedPromise;
  }

  // Set the value that will be retuned when `onAcceleratorsUpdated()` is
  // called.
  setFakeAcceleratorsUpdated(config: AcceleratorConfig[]): void {
    this.observables.setObservableData(
        ON_ACCELERATORS_UPDATED_METHOD_NAME, config);
  }

  addAccelerator(
      _source: AcceleratorSource, _actionId: number,
      _accelerator: Accelerator): Promise<{result: AcceleratorResultData}> {
    return this.methods.resolveMethod('addAccelerator');
  }

  replaceAccelerator(
      _source: AcceleratorSource, _actionId: number,
      _old_accelerator: Accelerator,
      _new_accelerator: Accelerator): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    return this.methods.resolveMethod('replaceAccelerator');
  }

  removeAccelerator(): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    const result = new AcceleratorResultData();
    result.result = AcceleratorConfigResult.kSuccess;
    this.methods.setResult('removeAccelerator', {result});
    return this.methods.resolveMethod('removeAccelerator');
  }

  restoreDefault(_source: AcceleratorSource, _actionId: number):
      Promise<{result: AcceleratorResultData}> {
    ++this.restoreDefaultCallCount;
    // Always return kSuccess in this fake.
    const result = new AcceleratorResultData();
    result.result = AcceleratorConfigResult.kSuccess;
    this.methods.setResult('restoreDefault', {result});
    return this.methods.resolveMethod('restoreDefault');
  }

  restoreAllDefaults(): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    const result = new AcceleratorResultData();
    result.result = AcceleratorConfigResult.kSuccess;
    this.methods.setResult('restoreAllDefaults', {result});
    return this.methods.resolveMethod('restoreAllDefaults');
  }

  preventProcessingAccelerators(_preventProcessingAccelerators: boolean):
      Promise<void> {
    ++this.preventProcessingAcceleratorsCallCount;
    return this.methods.resolveMethod('preventProcessingAccelerators');
  }

  /**
   * Sets the value that will be returned when calling
   * getAccelerators().
   */
  setFakeAcceleratorConfig(config: MojoAcceleratorConfig): void {
    this.methods.setResult('getAccelerators', {config});
  }

  /**
   * Sets the value that will be returned when calling
   * getAcceleratorLayoutInfos().
   */
  setFakeAcceleratorLayoutInfos(layoutInfos: MojoLayoutInfo[]): void {
    this.methods.setResult('getAcceleratorLayoutInfos', {layoutInfos});
  }

  getRestoreDefaultCallCount(): number {
    return this.restoreDefaultCallCount;
  }

  getPreventProcessingAcceleratorsCallCount(): number {
    return this.preventProcessingAcceleratorsCallCount;
  }

  setFakeHasLauncherButton(hasLauncherButton: boolean): void {
    this.methods.setResult('hasLauncherButton', {hasLauncherButton});
  }

  setFakeAddAcceleratorResult(result: AcceleratorResultData): void {
    this.methods.setResult('addAccelerator', {result});
  }

  setFakeReplaceAcceleratorResult(result: AcceleratorResultData): void {
    this.methods.setResult('replaceAccelerator', {result});
  }

  // Sets up an observer for methodName.
  private observe(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      resolve();
    });
  }
}
