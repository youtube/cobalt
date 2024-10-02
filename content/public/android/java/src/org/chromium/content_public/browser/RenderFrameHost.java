// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.mojo.bindings.Interface;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.List;

/**
 * The RenderFrameHost Java wrapper to allow communicating with the native RenderFrameHost object.
 */
public interface RenderFrameHost {
    /** The results of {@link #GetAssertionWebAuthSecurityChecks}. */
    final class WebAuthSecurityChecksResults {
        public final boolean isCrossOrigin;
        public final @AuthenticatorStatus.EnumType int securityCheckResult;

        /**
         * Creates an instance of this class.
         * @param securityCheckResult The status code indicating the result of the GetAssertion
         *        request security checks.
         * @param isCrossOrigin Whether the given origin is cross-origin with any frame in the
         *        current frame's ancestor chain.
         */
        public WebAuthSecurityChecksResults(
                @AuthenticatorStatus.EnumType int securityCheckResult, boolean isCrossOrigin) {
            this.securityCheckResult = securityCheckResult;
            this.isCrossOrigin = isCrossOrigin;
        }
    }

    /**
     * Get the last committed URL of the frame.
     *
     * @return The last committed URL of the frame or null when being destroyed.
     */
    @Nullable
    GURL getLastCommittedURL();

    /**
     * Get the last committed Origin of the frame. This is not always the same as scheme/host/port
     * of getLastCommittedURL(), since it can be an "opaque" origin in such cases as, for example,
     * sandboxed frame.
     *
     * @return The last committed Origin of the frame or null when being destroyed.
     */
    @Nullable
    Origin getLastCommittedOrigin();

    /**
     * Fetch the canonical URL associated with the fame.
     *
     * @param callback The callback to be notified once the canonical URL has been fetched.
     */
    void getCanonicalUrlForSharing(Callback<GURL> callback);

    /**
     * Fetch all RenderFramesHosts from the current frame.
     *
     * @return A list of RenderFramesHosts including the current frame and all descendents.
     */
    public List<RenderFrameHost> getAllRenderFrameHosts();

    /**
     * Returns whether the feature policy allows the feature in this frame.
     *
     * @param feature A feature controlled by feature policy.
     *
     * @return Whether the feature policy allows the feature in this frame.
     */
    boolean isFeatureEnabled(@PermissionsPolicyFeature int feature);

    /**
     * Returns an interface by name to the Frame in the renderer process. This
     * provides access to interfaces implemented in the renderer to Java code in
     * the browser process.
     *
     * Callers are responsible to ensure that the renderer Frame exists before
     * trying to make a mojo connection to it. This can be done via
     * isRenderFrameLive() if the caller is not inside the call-stack of an
     * IPC form the renderer (which would guarantee its existence at that time).
     */
    <I extends Interface, P extends Interface.Proxy> P getInterfaceToRendererFrame(
            Interface.Manager<I, P> manager);

    /**
     * Kills the renderer process when it is detected to be misbehaving and has
     * made a bad request.
     *
     * @param reason The BadMessageReason code from content::bad_message::BadMessageReason.
     */
    void terminateRendererDueToBadMessage(int reason);

    /**
     * Notifies the native RenderFrameHost about a user activation from the browser side.
     */
    void notifyUserActivation();

    /**
     * If a CloseWatcher is active in this RenderFrameHost, signal it to close.
     * @return Whether a close signal was sent.
     */
    boolean signalCloseWatcherIfActive();

    /**
     * Returns whether we're in incognito mode.
     *
     * @return {@code true} if we're in incognito mode.
     */
    boolean isIncognito();

    /**
     * See native RenderFrameHost::IsRenderFrameLive().
     *
     * @return {@code true} if render frame is created.
     */
    boolean isRenderFrameLive();

    /**
     * @return Whether input events from the renderer are ignored on the browser side.
     */
    boolean areInputEventsIgnored();

    /**
     * Runs security checks associated with a Web Authentication GetAssertion request for the
     * the given relying party ID and an effective origin. If the request originated from a render
     * process, then the effective origin is the same as the last committed origin. However, if the
     * request originated from an internal request from the browser process (e.g. Payments
     * Autofill), then the relying party ID would not match the renderer's origin, and will
     * therefore have to provide its own effective origin. `isPaymentCredentialGetAssertion`
     * indicates whether the security check is done for getting an assertion for Secure Payment
     * Confirmation (SPC). The return value is a code corresponding to the AuthenticatorStatus
     * mojo enum.
     *
     * @return An object containing (1) the status code indicating the result of the GetAssertion
     *         request security checks. (2) whether the effectiveOrigin is a cross-origin with any
     *         frame in this frame's ancestor chain.
     */
    WebAuthSecurityChecksResults performGetAssertionWebAuthSecurityChecks(
            String relyingPartyId, Origin effectiveOrigin, boolean isPaymentCredentialGetAssertion);

    /**
     * Runs security checks associated with a Web Authentication MakeCredential request for the
     * the given relying party ID, an effective origin and whether MakeCredential is making the
     * payment credential. See
     * performGetAssertionWebAuthSecurityChecks for more on |effectiveOrigin|. The return value is a
     * code corresponding to the AuthenticatorStatus mojo enum.
     *
     * @return Status code indicating the result of the MakeCredential request security checks.
     */
    int performMakeCredentialWebAuthSecurityChecks(
            String relyingPartyId, Origin effectiveOrigin, boolean isPaymentCredentialCreation);

    /**
     * @return An identifier for this RenderFrameHost.
     */
    GlobalRenderFrameHostId getGlobalRenderFrameHostId();

    /**
     * Returns the LifecycleState associated with this RenderFrameHost.
     * Features that display UI to the user (or cross document/tab boundary in
     * general, e.g. when using WebContents::FromRenderFrameHost) should first
     * check whether the RenderFrameHost is in the appropriate lifecycle state.
     *
     * @return The LifecycleState associated with this RenderFrameHost.
     */
    @LifecycleState
    int getLifecycleState();

    /**
     * Inserts a VisualStateCallback that's resolved once a visual update has been processed.
     *
     * The VisualStateCallback will be inserted in Blink and will be invoked when the contents of
     * the DOM tree at the moment that the callback was inserted (or later) are submitted to the
     * compositor in a CompositorFrame. In other words, the following events need to happen before
     * the callback is invoked:
     * 1. The DOM tree is committed becoming the pending tree - see ThreadProxy::BeginMainFrame
     * 2. The pending tree is activated becoming the active tree
     * 3. The compositor calls Draw and submits a new CompositorFrame to the Viz process.
     * The callback is synchronously invoked if this is called while being destroyed.
     *
     * @param callback the callback to be inserted. The callback takes a single Boolean parameter
     *     which will be true if the visual state update was successful or false if it was aborted.
     */
    void insertVisualStateCallback(Callback<Boolean> callback);
}
