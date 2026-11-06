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

The `Codec` class represents the configuration of a particular payload type as
represented in an SDP offer/answer, which also controls how the data is
transferred on the wire. A better name would have been something like
`RtpPayloadTypeConfiguration`, but the `Codec` name is embedded quite widely in
the code.

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

```
          +-----------------------------------+
          | PeerConnection's PayloadTypePicker|
          +-----------------+-----------------+
                            |
              +-------------+-------------+
              |                           |
      +-------v-------+           +-------v-------+
      |  VoiceEngine  |           |  VideoEngine  |
      +-------+-------+           +-------+-------+
              |                           |
              |      (Initial PTs)        |
              v                           v
      +-------------------------------------------+    +-----------------------+
      |               CodecVendor                 |    |    Media section's    |
      |      (Munges PTs to avoid collisions)     +---->   PayloadTypePicker   |
      +-------+-------------+-------+-------------+    +-----------+-----------+
              |             |       |                              |
              |             |       |              +---------------v-------+
              |             |       +-------------->SdpPayloadTypeSuggester|
              |             |                      +---------------+-------+
              |             |                                      |
              v             v                                      v
      +----------------+ +------------------+<---------------------+
      |TypedCodecVendor| |PayloadTypeRecorder|
      +----------------+ +------------------+
```

## Desired future strategy

There should be no assignment of payload types in VideoEngine and VoiceEngine.
Payload types should be picked only when creating an offer or answer; they are
assigned permanently in SetLocalDescription / SetRemoteDescription.

Once an assignment is made, it should never need to change (apart from the case
noted above).

```
          +-------------+           +-------------+
          | VoiceEngine |           | VideoEngine |
          +------+------+           +------+------+
                 |                         |
                 |  (CodecConfigurations)  |
                 |    (No initial PTs)     |
                 v                         v
          +-------------------------------------------+    +-----------------------+
          |               CodecVendor                 |    |    Media section's    |
          |   (Assigns PTs and expands resiliency)    +---->   PayloadTypePicker   |
          +-------+-----------------------------------+    +-----------+-----------+
                  |                                                    |
                  v                                        +-----------v-----------+
          +----------------+                               |SdpPayloadTypeSuggester|
          |TypedCodecVendor|                               +-----------+-----------+
          +----------------+                                           |
                                                           +-----------v-----------+
                                                           | PayloadTypeRecorder & |
                                                           | PeerConnection's PT   |
                                                           | Picker                |
                                                           +-----------------------+
```

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

## Unified Implementation Strategy for Audio and Video

The goal is to transition audio and video codec handling to a unified
late-assignment model using a new internal representation to handle resiliency
mechanisms. This also involves refactoring the existing partial late assignment
implementation for audio.

### 1. CodecConfiguration and ResiliencyInfo

To support late assignment without modifying the global `webrtc::Codec` class, a
new internal representation `CodecConfiguration` will be introduced in the `pc/`
directory.

- **`ResiliencyInfo`**: Encapsulates the redundancy requirements for a codec
  (e.g., RTX, RED, ULPFEC, FlexFEC). It supports combined requirements (RED +
  ULPFEC) and identifies whether a mechanism is shared across the media section.
- **`CodecConfiguration`**: Stores codec attributes (excluding payload type) and
  the associated `ResiliencyInfo`. This is the primary representation used
  during capability gathering and the initial stages of negotiation.

### 2. Unified Codec Collection with Bifurcated Paths

- `TypedCodecVendor` will be updated to store either a legacy `CodecList` or a
  collection of `CodecConfiguration` objects for audio and video, depending on
  the `WebRTC-PayloadTypesInTransport` field trial.
- When the trial is active:
  - **Audio**: `CollectAudioCodecs` will be refactored to return
    `CodecConfiguration` objects. Media codecs like Opus will be tagged with a
    shared RED requirement.
  - **Video**: `CollectVideoCodecs` and `VideoCodecsFromFactory` will populate
    `CodecConfiguration` objects, tagging media codecs with their required
    resiliency (e.g., VP8 gets RTX; all video codecs get shared RED and
    FlexFEC).
- When the trial is inactive, legacy methods will be used to ensure zero
  behavior change.

### 3. Late Expansion and Unified Parameter Linking

`CodecVendor` will bifurcate its negotiation logic:

- **Legacy Path**: Continues to use the existing `MergeCodecs` logic with
  pre-assigned payload types.
- **Late Assignment Path**: Uses a new `MergeCodecsFromConfigurations` function
  for all media types that:
  1. Assigns a payload type to the primary media codec via
     `SdpPayloadTypeSuggester`.
  2. Expands the `ResiliencyInfo` into one or more redundancy `Codec` objects.
  3. Links these redundancy codecs to the primary codec's payload type (e.g.,
     setting the `apt` parameter for RTX, or updating RED's FMTP with the
     primary PT).
  4. Assigns payload types to the redundancy codecs, following conventional
     rules where possible (e.g., `RTX_PT = Primary_PT + 1`).

This unified strategy removes the need for media-specific hacks (like the
current manual RED linking for audio) and ensures that all redundancy codecs are
correctly linked only after the primary payload types are known, while strictly
preserving legacy behavior when the field trial is disabled.

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
