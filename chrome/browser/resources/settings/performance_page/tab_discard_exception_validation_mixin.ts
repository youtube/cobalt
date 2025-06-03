// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';

export const MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH = 10 * 1024;

export const TAB_DISCARD_EXCEPTIONS_PREF =
    'performance_tuning.tab_discarding.exceptions';
export const TAB_DISCARD_EXCEPTIONS_MANAGED_PREF =
    'performance_tuning.tab_discarding.exceptions_managed';

type Constructor<T> = new (...args: any[]) => T;

export const TabDiscardExceptionValidationMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<TabDiscardExceptionValidationMixinInterface&
                I18nMixinInterface> => {
      const superClassBase = I18nMixin(superClass);
      class TabDiscardExceptionValidationMixin extends superClassBase implements
          TabDiscardExceptionValidationMixinInterface {
        static get properties() {
          return {
            errorMessage: {type: String, value: ''},
            inputInvalid: {type: Boolean, value: false},
            rule: String,
            submitDisabled: {type: Boolean, value: true, notify: true},
          };
        }

        private browserProxy_: PerformanceBrowserProxy =
            PerformanceBrowserProxyImpl.getInstance();
        errorMessage: string;
        inputInvalid: boolean;
        rule: string;
        submitDisabled: boolean;

        validate() {
          const rule = this.rule.trim();

          if (!rule) {
            this.inputInvalid = false;
            this.submitDisabled = true;
            this.errorMessage = '';
            return;
          }

          if (rule.length > MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH) {
            this.inputInvalid = true;
            this.submitDisabled = true;
            this.errorMessage = this.i18n('onStartupUrlTooLong');
            return;
          }

          this.browserProxy_.validateTabDiscardExceptionRule(rule).then(
              valid => {
                this.inputInvalid = !valid;
                this.submitDisabled = !valid;
                this.errorMessage =
                    valid ? '' : this.i18n('onStartupInvalidUrl');
              });
        }
      }

      return TabDiscardExceptionValidationMixin;
    });

export interface TabDiscardExceptionValidationMixinInterface {
  errorMessage: string;
  inputInvalid: boolean;
  rule: string;
  submitDisabled: boolean;
  validate(): void;
}
