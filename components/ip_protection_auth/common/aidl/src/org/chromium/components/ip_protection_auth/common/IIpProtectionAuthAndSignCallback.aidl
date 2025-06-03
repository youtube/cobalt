// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.common;

/**
 * Used for communicating the result of authAndSign() to the caller.
 */
oneway interface IIpProtectionAuthAndSignCallback {
    // Parameter is the serialized form of AuthAndSignResponse proto
    void reportResult(in byte[] response) = 0;
    // Parameter is the serialized form of AuthAndSignError proto
    void reportError(in byte[] error) = 1;
}
