// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generate this token with the command:
// tools/origin_trials/generate_token.py https://example.test Tpcd --version 3 --is-third-party --expire-timestamp=2000000000
const THIRD_PARTY_TOKEN = 'A4R02lBma1/SU5GBtWzt4xAJhrzX6r8K1YTPkOSIY8Y0rv4KbrJNumvUfQxGsJJ+Xcu+yTwdB5DG9F31rQACvQMAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiVHBjZCIsICJleHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=';

const tokenElement = document.createElement('meta');
tokenElement.httpEquiv = 'origin-trial';
tokenElement.content = THIRD_PARTY_TOKEN;
document.head.appendChild(tokenElement);