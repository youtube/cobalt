// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.url.Origin;

/** A mock Fido2CredentialRequest that returns NOT_IMPLEMENTED for all calls. */
public class MockFido2CredentialRequest extends Fido2CredentialRequest {
    public MockFido2CredentialRequest() {
        super(null, WebAuthenticationDelegate.Support.BROWSER);
    }

    @Override
    public void handleMakeCredentialRequest(PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, MakeCredentialResponseCallback callback,
            FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    public void handleGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, Origin callerOrigin, PaymentOptions payment,
            GetAssertionResponseCallback callback, FidoErrorResponseCallback errorCallback) {
        errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
    }

    @Override
    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            RenderFrameHost frameHost, IsUvpaaResponseCallback callback) {
        callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
    }
}
