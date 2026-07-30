// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PrefService} from './pref_service.js';

type PrefObject<T> = chrome.settingsPrivate.PrefObject<T>;
type ObserverCallback<T> = (pref: Readonly<PrefObject<T>>) => void;

type Constructor<T> = new (...args: any[]) => T;

export const PrefServiceObserverMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<PrefServiceObserverMixinLitInterface> => {
      class PrefServiceObserverMixinLit extends superClass implements
          PrefServiceObserverMixinLitInterface {
        private prefServiceObserverIds_: number[] = [];

        override disconnectedCallback() {
          super.disconnectedCallback();
          for (const id of this.prefServiceObserverIds_) {
            PrefService.getInstance().removeObserver(id);
          }
          this.prefServiceObserverIds_ = [];
        }

        addPrefObserver<T>(prefKey: string, callback: ObserverCallback<T>):
            number {
          const id =
              PrefService.getInstance().addObserver<T>(prefKey, callback);
          this.prefServiceObserverIds_.push(id);
          return id;
        }

        removePrefObserver(id: number) {
          PrefService.getInstance().removeObserver(id);
          const index = this.prefServiceObserverIds_.indexOf(id);
          if (index !== -1) {
            this.prefServiceObserverIds_.splice(index, 1);
          }
        }

        mirrorPref(prefKey: string, propName: string) {
          this.addPrefObserver(prefKey, pref => {
            (this as unknown as Record<string, unknown>)[propName] = pref;
          });
        }

        mirrorPrefs(prefMap: Record<string, string>) {
          for (const [prefKey, propName] of Object.entries(prefMap)) {
            this.mirrorPref(prefKey, propName);
          }
        }
      }

      return PrefServiceObserverMixinLit;
    };

export interface PrefServiceObserverMixinLitInterface {
  addPrefObserver<T>(prefKey: string, callback: ObserverCallback<T>): number;
  removePrefObserver(id: number): void;
  mirrorPref(prefKey: string, propName: string): void;
  mirrorPrefs(prefMap: Record<string, string>): void;
}
