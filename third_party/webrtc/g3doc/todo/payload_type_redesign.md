# Payload type allocation redesign

## Background: What a payload type is

The payload type is a property of a codec that is established between a sender
and a receiver using SDP offer/answer.

Each end of the connection independently indicates the list of codecs it is
willing to send and/or receive, and assigns a payload type to each. It is
conventional but not required to use the same payload types in an answer as
those suggested in an offer.

For SDP sendonly media sections, it indicates a willingness to send; for
recvonly media sections, it indicates a willingness to receive; for sendrecv
media sections, it indicates a willingness to receive AND a willingness to send.
Presence in a media section does not guarantee that it will be used. (RFC 3264)

## Current allocation strategy

The payload types are assigned by the VideoEngine and VoiceEngine according to a
fixed list. If the assignments collide with already established payload types,
the assignments are munged in the CodecVendor class so that there are no
colliding payload types.

The PayloadTypeRecorder class records what payload types are assigned - there is
one case (PR-answer followed by a later Answer that has different PTs) where
they are changed, but otherwise, once the PayloadTypeRecorder has assigned them,
that PT is permanent for that transport and that direction.

Picking a payload type is done in the PayloadTypePicker class. This is preloaded
with some assignments, so that if a PT is free, the commonly used PT for that
codec can be used.

## Desired future strategy

There should be no assignment of payload types in VideoEngine and VoiceEngine.
Payload types should be picked only when creating an offer or answer; they are
assigned permanently in SetLocalDescription / SetRemoteDescription.

Once an assignment is made, it should never need to change (apart from the case
noted above).

## Dealing with resiliency mechanisms

Resiliency mechanisms (RED, RTX) are signaled using "codecs", and are
traditionally represented as such within the library. They associate with the
codec they are resiliency for by using the "apt=" parameter in their a=fmtp
lines. However, this complicates assignment, since that parameter cannot be
filled in until the payload type for the "protected" codec is assigned.

## Backwards compatibility issues

Experience has shown that changing the PT allocation strategy often trips up
applications that have depended on the result of the old allocation strategy, so
user-visible changes should be avoided. One example is that it's preferred (but
not strictly required) to have the RTX PT assigned the PT number of 1 more than
the PT of the codec it refers to.

## Current implementation status

The new strategy is implemented for audio codecs. Several issues that caused
test failures when enabling the `WebRTC-PayloadTypesInTransport` field trial
have been identified and fixed:

- **Audio/Video RED Collision:** RED codecs of different media types were
  incorrectly matching, leading to payload type conflicts.
  `MatchesWithCodecRules` now enforces media type equality.
- **MID Recycling:** When a MID was recycled for a different media type (e.g.,
  Audio -> Video), `CodecVendor` was incorrectly merging codecs from the old
  description. This has been fixed by validating the media type before merging.
- **RED Matching Logic:** Relaxed the matching rules for RED to allow
  negotiation to proceed even when parameters (linking RED to primary codecs)
  are not yet populated, as this linking now happens late in the `CodecVendor`.

The new strategy is not yet implemented for video codecs.

## Implementation Strategy for Video

The goal is to transition video codec handling to the same late-assignment model
used for audio.

### 1. Unified Codec Collection

- Implement `VideoCodecsFromFactory` in `pc/typed_codec_vendor.cc`. This will
  query the `VideoEncoderFactory` and `VideoDecoderFactory` to gather supported
  `SdpVideoFormat`s.
- These codecs will be initialized with `kIdNotSet`, allowing
  `SdpPayloadTypeSuggester` to assign payload types only during the creation of
  an offer or answer.

### 2. Payload Type Suggester Integration

- Ensure `PayloadTypePicker` has a complete list of preferred payload types for
  video codecs (VP8, VP9, H.264, AV1, etc.) to maintain stable and conventional
  assignments.
- Update `CodecVendor` to use the unassigned video codec list when the
  `WebRTC-PayloadTypesInTransport` trial is active.

### 3. Resiliency and Parameter Linking

- Refine `MergeCodecs` and `AssignCodecIdsAndLinkRed` to handle video-specific
  resiliency:
  - **RTX:** Correctly link RTX codecs to their primary video codecs by matching
    names and specific parameters (like `profile-level-id` for H.264).
  - **RED/ULPFEC:** Ensure parameters for video redundancy are correctly
    populated once the primary codec PTs are assigned.

### 4. Verification and Testing

- **Integration Tests:** Enable the `WebRTC-PayloadTypesInTransport` trial in
  `peerconnection_unittests` and `rtc_unittests` to identify any video-specific
  regressions.
- **Stable PT Tests:** Add coverage to ensure that payload types remain stable
  across renegotiations, even when the order of codecs in the transceiver
  preferences changes.
- **MID Recycling:** Verify that MID recycling for video-to-audio and vice-versa
  works correctly without PT collisions or crashes.

## Desired next steps

Analyze the current situation. (Done)

### Test, isolate and fix failures

The identified failures in `PeerConnectionEncodingsIntegrationTest` and
`PeerConnectionIntegrationTest` have been resolved. Next, focus on implementing
the video strategy outlined above.
