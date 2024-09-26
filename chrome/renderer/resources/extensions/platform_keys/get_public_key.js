// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var internalAPI = getInternalApi('platformKeysInternal');

var normalizeAlgorithm =
    requireNative('platform_keys_natives').NormalizeAlgorithm;

// Returns the normalized parameters of |importParams|.
// Any unknown parameters will be ignored.
function normalizeImportParams(importParams) {
  if (!importParams.name || typeof importParams.name !== 'string') {
    throw $Error.self('Algorithm: name: Missing or not a String');
  }

  var filteredParams = {
    name: importParams.name,
    namedCurve: importParams.namedCurve
  };

  var hashIsNone = false;
  if (importParams.hash) {
    if (importParams.hash.name.toLowerCase() === 'none') {
      hashIsNone = true;
      // Temporarily replace |hash| by a valid WebCrypto Hash for normalization.
      // This will be reverted to 'none' after normalization.
      filteredParams.hash = { name: 'SHA-1' };
    } else {
      filteredParams.hash = { name: importParams.hash.name }
    }
  }

  if (importParams.name === 'ECDSA' && importParams.namedCurve !== 'P-256') {
    throw $Error.self('Only P-256 named curve is supported.');
  }

  // Apply WebCrypto's algorithm normalization.
  var resultParams = normalizeAlgorithm(filteredParams, 'ImportKey');
  if (!resultParams) {
    throw $Error.self('A required parameter was missing or out-of-range');
  }
  if (hashIsNone) {
    resultParams.hash = { name: 'none' };
  }
  return resultParams;
}

function combineAlgorithms(algorithm, importParams) {
  // internalAPI.getPublicKey returns publicExponent as ArrayBuffer, but it
  // should be a Uint8Array.
  if (algorithm.publicExponent) {
    algorithm.publicExponent = new Uint8Array(algorithm.publicExponent);
  }

  if (importParams.hash) {
    algorithm.hash = importParams.hash;
  }
  return algorithm;
}

function getPublicKey(cert, importParams, callback) {
  // TODO(crbug.com/1096486): Check cert type is ArrayBuffer.
  importParams = normalizeImportParams(importParams);
  internalAPI.getPublicKey(
      cert, importParams.name, function(publicKey, algorithm) {
        if (chrome.runtime.lastError) {
          callback();
          return;
        }
        var combinedAlgorithm = combineAlgorithms(algorithm, importParams);
        callback(publicKey, combinedAlgorithm);
      });
}

function getPublicKeyBySpki(publicKeySpkiDer, importParams, callback) {
  if (!(publicKeySpkiDer instanceof ArrayBuffer)){
    throw $Error.self('publicKeySpkiDer: Not an ArrayBuffer');
  }
  importParams = normalizeImportParams(importParams);
  internalAPI.getPublicKeyBySpki(
      publicKeySpkiDer, importParams.name, function(publicKey, algorithm) {
        if (bindingUtil.hasLastError()) {
          callback();
          return;
        }
        var combinedAlgorithm = combineAlgorithms(algorithm, importParams);
        callback(publicKey, combinedAlgorithm);
      });
}

exports.$set('getPublicKey', getPublicKey);
exports.$set('getPublicKeyBySpki', getPublicKeyBySpki);
