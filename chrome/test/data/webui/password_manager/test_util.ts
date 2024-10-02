// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PasswordCheckParams {
  state?: chrome.passwordsPrivate.PasswordCheckState;
  totalNumber?: number;
  checked?: number;
  remaining?: number;
  lastCheck?: string;
}

export function makePasswordCheckStatus(params: PasswordCheckParams):
    chrome.passwordsPrivate.PasswordCheckStatus {
  return {
    state: params.state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    totalNumberOfPasswords: params.totalNumber,
    alreadyProcessed: params.checked,
    remainingInQueue: params.remaining,
    elapsedTimeSinceLastCheck: params.lastCheck,
  };
}

export interface PasswordEntryParams {
  url?: string;
  username?: string;
  password?: string;
  federationText?: string;
  id?: number;
  inAccountStore?: boolean;
  inProfileStore?: boolean;
  isAndroidCredential?: boolean;
  note?: string;
  affiliatedDomains?: chrome.passwordsPrivate.DomainInfo[];
}

/**
 * Creates a single item for the list of passwords, in the format sent by the
 * password manager native code. If no |params.id| is passed, it is set to a
 * default, value so this should probably not be done in tests with multiple
 * entries (|params.id| is unique). If no |params.frontendId| is passed, it is
 * set to the same value set for |params.id|.
 */
export function createPasswordEntry(params?: PasswordEntryParams):
    chrome.passwordsPrivate.PasswordUiEntry {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  // Fallback to device store if no parameter provided.
  let storeType: chrome.passwordsPrivate.PasswordStoreSet =
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

  if (params.inAccountStore && params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT;
  } else if (params.inAccountStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
  } else if (params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
  }
  const note = params.note || '';

  return {
    urls: {
      signonRealm: 'https://' + url + '/login',
      shown: url,
      link: 'https://' + url + '/login',
    },
    username: username,
    federationText: params.federationText,
    id: id,
    storedIn: storeType,
    isAndroidCredential: params.isAndroidCredential || false,
    note: note,
    password: params.password || '',
    affiliatedDomains: params.affiliatedDomains,
  };
}

export interface CredentialGroupParams {
  name?: string;
  icon?: string;
  credentials?: chrome.passwordsPrivate.PasswordUiEntry[];
}

export function createCredentialGroup(params?: CredentialGroupParams):
    chrome.passwordsPrivate.CredentialGroup {
  params = params || {};
  return {
    name: params.name || '',
    iconUrl: params.icon || '',
    entries: params.credentials || [],
  };
}

/**
 * Creates a single item for the list of password blockedSites. If no |id| is
 * passed, it is set to a default, value so this should probably not be done in
 * tests with multiple entries (|id| is unique).
 */
export function createBlockedSiteEntry(
    url?: string, id?: number): chrome.passwordsPrivate.ExceptionEntry {
  url = url || 'www.foo.com';
  id = id || 42;
  return {
    urls: {
      signonRealm: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
}

export function makePasswordManagerPrefs() {
  return {
    credentials_enable_service: {
      key: 'credentials_enable_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    credentials_enable_autosignin: {
      key: 'credentials_enable_autosignin',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    profile: {
      password_dismiss_compromised_alert: {
        key: 'profile.password_dismiss_compromised_alert',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    // <if expr="is_win or is_macosx">
    password_manager: {
      biometric_authentication_filling: {
        key: 'password_manager.biometric_authentication_filling',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    // </if>
  };
}

export interface InsecureCredentialsParams {
  url?: string;
  username?: string;
  password?: string;
  types?: chrome.passwordsPrivate.CompromiseType[];
  id?: number;
  elapsedMinSinceCompromise?: number;
  isMuted?: boolean;
}

/**
 * Creates a new insecure credential.
 */
export function makeInsecureCredential(params: InsecureCredentialsParams):
    chrome.passwordsPrivate.PasswordUiEntry {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  const elapsedMinSinceCompromise = params.elapsedMinSinceCompromise || 0;
  const types = params.types || [];
  const compromisedInfo = {
    compromiseTime: Date.now() - (elapsedMinSinceCompromise * 60000),
    elapsedTimeSinceCompromise: `${elapsedMinSinceCompromise} minutes ago`,
    compromiseTypes: types,
    isMuted: params.isMuted ?? false,
  };
  return {
    id: id || 0,
    storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    changePasswordUrl: `https://${url}/`,
    urls: {
      signonRealm: `https://${url}/`,
      shown: url,
      link: `https://${url}/`,
    },
    username: username,
    password: params.password,
    note: '',
    isAndroidCredential: false,
    compromisedInfo: types.length ? compromisedInfo : undefined,
  };
}

export function createAffiliatedDomain(domain: string):
    chrome.passwordsPrivate.DomainInfo {
  return {
    name: domain,
    url: `https://${domain}/login`,
    signonRealm: `https://${domain}/login`,
  };
}
