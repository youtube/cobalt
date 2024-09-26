// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DeviceTrustState, Int32Value, KeyInfo, KeyManagerInitializedValue, KeyManagerPermanentFailure, KeyTrustLevel, KeyType, PageHandler, PageHandlerInterface} from './connectors_internals.mojom-webui.js';
import {getTemplate} from './device_trust_connector.html.js';

const TrustLevelStringMap = {
  [KeyTrustLevel.UNSPECIFIED]: 'Unspecified',
  [KeyTrustLevel.HW]: 'HW',
  [KeyTrustLevel.OS]: 'OS',
};

const KeyTypeStringMap = {
  [KeyType.UNKNOWN]: 'Unknown',
  [KeyType.RSA]: 'RSA',
  [KeyType.EC]: 'EC',
};

const KeyPermanentFailureMap = {
  [KeyManagerPermanentFailure.CREATION_UPLOAD_CONFLICT]:
      'A key already exists on the server for this device.',
  [KeyManagerPermanentFailure.INSUFFICIENT_PERMISSIONS]:
      'The browser is missing permissions and is unable to create a Device ' +
      'Trust key.',
  [KeyManagerPermanentFailure.OS_RESTRICTION]:
      'This device is missing a critical feature (e.g. no SecureEnclave ' +
      'support on Mac).',
  [KeyManagerPermanentFailure.INVALID_INSTALLATION]:
      'The browser is missing a critical installation dependency (e.g. is ' +
      'a user-level installation on Windows).',
};

export class DeviceTrustConnectorElement extends CustomElement {
  static get is() {
    return 'device-trust-connector';
  }

  static override get template() {
    return getTemplate();
  }

  public set enabledString(str: string) {
    const strEl = (this.$('#enabled-string') as HTMLElement);
    if (strEl) {
      strEl.innerText = str;
    } else {
      console.error('Could not find #enabled-string element.');
    }
  }

  public set keyInfo(keyInfo: KeyInfo) {
    const keySectionEl = (this.$('#key-manager-section') as HTMLElement);
    const initStateEl = (this.$('#key-manager-state') as HTMLElement);

    const keyPermanentErrorRowEl =
        (this.$('#key-permanent-failure-row') as HTMLElement);
    const keyPermanentErrorValueEl =
        (this.$('#key-permanent-failure') as HTMLElement);

    const keyLoadedRows = (this.$('#key-loaded-rows') as HTMLElement);
    const trustLevelStateEl = (this.$('#key-trust-level') as HTMLElement);
    const keyTypeStateEl = (this.$('#key-type') as HTMLElement);
    const spkiHashStateEl = (this.$('#spki-hash') as HTMLElement);
    const keySyncStateEl = (this.$('#key-sync') as HTMLElement);

    const initializedValue = keyInfo.isKeyManagerInitialized;
    if (initializedValue === KeyManagerInitializedValue.UNSUPPORTED) {
      this.hideElement(keySectionEl);
    } else {
      const keyLoaded =
          initializedValue === KeyManagerInitializedValue.KEY_LOADED;
      initStateEl.innerText = keyLoaded ? 'true' : 'false';
      this.showElement(keySectionEl);

      if (keyInfo.permanentFailure === KeyManagerPermanentFailure.UNSPECIFIED) {
        this.hideElement(keyPermanentErrorRowEl);
      } else {
        const permanentFailureMessage =
            KeyPermanentFailureMap[keyInfo.permanentFailure] ||
            `Unknown: ${keyInfo.permanentFailure}`;
        keyPermanentErrorValueEl.innerText = permanentFailureMessage;

        this.showElement(keyPermanentErrorRowEl);
      }

      const keyMetadata = keyInfo.loadedKeyInfo;
      if (keyMetadata) {
        trustLevelStateEl.innerText =
            this.trustLevelToString(keyMetadata.trustLevel);
        keyTypeStateEl.innerText = this.keyTypeToString(keyMetadata.keyType);
        spkiHashStateEl.innerText = keyMetadata.encodedSpkiHash;
        keySyncStateEl.innerText =
            this.keySyncCodeToString(keyMetadata.syncKeyResponseCode);

        this.showElement(keyLoadedRows);
      } else {
        this.hideElement(keyLoadedRows);
      }
    }
  }

  private signalsString_: string = '';
  public set signalsString(str: string) {
    const signalsEl = (this.$('#signals') as HTMLElement);
    if (signalsEl) {
      signalsEl.innerText = str;
      this.signalsString_ = str;
    } else {
      console.error('Could not find #signals element.');
    }
  }

  public get copyButton(): HTMLButtonElement|undefined {
    return this.$('#copy-signals') as HTMLButtonElement;
  }

  public get signalsString(): string {
    return this.signalsString_;
  }

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();

    this.fetchDeviceTrustValues()
        .then(state => this.setDeviceTrustValues(state))
        .then(() => {
          const copyButton = this.copyButton;
          if (copyButton) {
            copyButton.addEventListener(
                'click', () => this.copySignals(copyButton));
          }
        });
  }

  private setDeviceTrustValues(state: DeviceTrustState|undefined) {
    if (!state) {
      this.enabledString = 'error';
      return;
    }

    this.enabledString = `${state.isEnabled}`;

    this.keyInfo = state.keyInfo;

    this.signalsString = state.signalsJson;
  }

  private async fetchDeviceTrustValues(): Promise<DeviceTrustState|undefined> {
    return this.pageHandler.getDeviceTrustState().then(
        (response: {state: DeviceTrustState}) => response && response.state,
        (e: object) => {
          console.warn(`fetchDeviceTrustValues failed: ${JSON.stringify(e)}`);
          return undefined;
        });
  }

  private async copySignals(copyButton: HTMLButtonElement): Promise<void> {
    copyButton.disabled = true;
    navigator.clipboard.writeText(this.signalsString)
        .finally(() => copyButton.disabled = false);
  }

  private showElement(element: Element) {
    element?.classList.remove('hidden');
  }

  private hideElement(element: HTMLElement) {
    element?.classList.add('hidden');
  }

  private trustLevelToString(trustLevel: KeyTrustLevel): string {
    return TrustLevelStringMap[trustLevel] || 'invalid';
  }

  private keyTypeToString(keyType: KeyType): string {
    return KeyTypeStringMap[keyType] || 'invalid';
  }

  private keySyncCodeToString(syncKeyResponseCode: Int32Value|
                              undefined): string {
    if (!syncKeyResponseCode) {
      return 'Undefined';
    }

    const value = syncKeyResponseCode.value;
    if (value / 100 === 2) {
      return `Success (${value})`;
    }
    return `Failure (${value})`;
  }
}

customElements.define(
    DeviceTrustConnectorElement.is, DeviceTrustConnectorElement);
