// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Message type used by the host to initiate the handshake ping. */
export const SKILLS_HANDSHAKE_TYPE = 'skills-handshake';

/** Message type used by the guest to acknowledge the handshake. */
export const SKILLS_HANDSHAKE_ACK = 'SKILLS_HANDSHAKE_ACK';

/**
 * Interval in milliseconds between successive handshake pings sent by the
 * host.
 */
export const HANDSHAKE_PING_INTERVAL_MS = 50;

/** Timeout in milliseconds before the host aborts the handshake. */
export const HANDSHAKE_TIMEOUT_MS = 5000;
