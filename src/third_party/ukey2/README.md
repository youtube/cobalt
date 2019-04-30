# Ukey2
This is not an officially supported Google product

**Coathored by:** Alexei Czeskis, Thai Duong, Eduardo' Vela'' \<Nava\>, and Adam Stubblefield.

**Status:** Implemented in Java (aczeskis@google.com)

**Design reviewers:** Thai Duong, Bruno Blanchet, Martin Abadi, and Bo Wang

**Implementation reviewer**: Thai Duong

**Last Updated:** roughly in September 2016



# Overview

UKEY2 is a Diffie-Hellman based authenticated key exchange protocol.

At the end of a UKEY2 run, a client and server have a shared master secret that can be used to
derive keys which can be used in a subsequent protocol. UKEY2 only implicitly guarantees that
servers know that clients believe the protocol finished correctly; that is, until a server
receives a message on the next protocol from the client it does not know that the handshake
completed.

The intended usage of UKEY2 is to establish a secure channel between two user devices,
e.g., laptop with Chromecast, phone with Google Glass, etc. The secure channel then can be used to
transmit passwords or other credentials. It is especially useful when one wants to connect a brand
 new device to a password-protected WIFI network.  UKEY2 is also usable over low-bandwidth
transports like Bluetooth Low Energy (see [Performance](#performance)).

# Message Framing

Each UKEY2 message is framed inside an outer protobuf message:


```
message Ukey2Message {
  enum Type {
    UNKNOWN_DO_NOT_USE = 0;
    ALERT = 1;
    CLIENT_INIT = 2;
    SERVER_INIT = 3;
    CLIENT_FINISH = 4;
  }

  optional Type message_type = 1;   // Identifies message type
  optional bytes message_data = 2;  // Actual message, to be parsed according to
                                    // message_type
}
```



# Alerts

In case an error occurs, the client and server will reply with an Alert:


```
message Ukey2Alert {
  enum AlertType {
    // Framing errors
    BAD_MESSAGE = 1;             // The message could not be deserialized
    BAD_MESSAGE_TYPE = 2;        // message_type has an undefined value
    INCORRECT_MESSAGE = 3;       // message_type received does not correspond to expected
                                 // type at this stage of the protocol
    BAD_MESSAGE_DATA = 4;        // Could not deserialize message_data as per value in
                                 // message_type

    // ClientInit and ServerInit errors
    BAD_VERSION = 100;           // version is invalid; server cannot find suitable version
                                 // to speak with client.
    BAD_RANDOM = 101;            // Random data is missing or of incorrect length
    BAD_HANDSHAKE_CIPHER = 102;  // No suitable handshake ciphers were found
    BAD_NEXT_PROTOCOL = 103;     // The next protocol is missing, unknown, or unsupported
    BAD_PUBLIC_KEY = 104;        // The public key could not be parsed

    // Other errors
    INTERNAL_ERROR = 200;       // An internal error has occurred.  error_message may
                                // contain additional details for logging and debugging.
  }

  optional AlertType type = 1;
  optional string error_message = 2;
}
```


The type corresponds to the error that caused the `Alert` to be sent. Upon encountering an error,
clients and servers send an Alert of the proper type and close the connection; all alerts are
fatal. Upon receiving an `Alert`, clients and servers must close the connection, even if they
cannot parse the `Alert`.  The `Alert` message may contain an optional `error_message` string
that may be used to describe error details for logging.

# Handshake Ciphersuites

UKEY2 supports negotiation of the cryptographic primitives used in the handshake. Two primitives
are required, a Diffie-Hellman function and a cryptographic hash function, which are represented
by a single enum:


```
enum Ukey2HandshakeCipher {
  RESERVED = 0;
  P256_SHA512 = 100;        // NIST P-256 used for ECDH, SHA512 used for commitment
  CURVE25519_SHA512 = 200;  // Curve 25519 used for ECDH, SHA512 used for commitment
}
```


The implementations of all primitives must resist timing side-channel attacks. A summary of
handshake ciphersuite negotiation is (see ClientInit and ServerInit messages for full details):

*   The client enumerates the primitives it supports and the server choose the highest (by enum value) cipher that it also supports.
*   The server replies with a public key using the chosen cipher and sends its own list of supported handshake cipher suites so that the client can verify that the right selection was made.


# Handshake Details

The UKEY2 handshake consists of three messages. First, the client sends a `ClientInit` message to
the server -- conceptually, this consists of a list of cipher suites and a commitment to an
ephemeral public key for each suite. The server responds with a `ServerInit` -- conceptually,
this is the server's chosen cipher suite and an ephemeral public key for the cipher suites
selected by the server.  Finally, the client responds with a `ClientFinished` -- conceptually,
this consists of an ephemeral public key matching the cipher suite selected by the server.

After the handshake, both client and server derive authentication strings, which may be shown to
users for visual comparison or sent over some other channel in order to authenticate the handshake.
The client and server also derive session keys for the next protocol.

## The `ClientInit` Message

The `ClientInit` message is defined as follows:


```
message Ukey2ClientInit {
  optional int32 version = 1;  // highest supported version for rollback protection
  optional bytes random = 2;   // random bytes for replay/reuse protection

  // One commitment (hash of ClientFinished containing public key) per supported cipher
  message CipherCommitment {
    optional Ukey2HandshakeCipher handshake_cipher = 1;
    optional bytes commitment = 2;
  }
  repeated CipherCommitment cipher_commitments = 3;

  // Next protocol that the client wants to speak.
  optional string next_protocol = 4;
}
```


The `version` field is the maximum version that the client supports.  It should be 1 for now. The `random` field is exactly 32 cryptographically secure random bytes.  The `cipher_commitment` field is a protobuf consisting of a handshake cipher and a commitment which is a hash of the `ClientFinished` message that would be sent if the cipher were selected (the serialized, including framing, raw bytes of the last handshake message sent by the client), calculated with the hash function and the Diffie-Hellman function from the handshake cipher. The client includes each commitment in the order of their preference.  Note that only one commitment per `handshake_cipher` is allowed.  The client also includes the `next_protocol` field that specifies that the client wants to use to speak to the server.  Note that this protocol must implicitly imply a key length.  UKEY2, however, does not provide a namespace for the `next_protocol` values in order to provide layers separation between the handshake and the next protocols.


## Interpreting `ClientInit`

Upon receiving the `ClientInit` message, the server should:



1.  Deserialize the protobuf; send an `Alert.BAD_MESSAGE` message if deserialization fails.
1.  Verify that `message_type == Type.CLIENT_INIT`; send an `Alert.BAD_MESSAGE_TYPE` message if mismatch occurs.
1.  Deserialize `message_data` as a `ClientInit` message; send an `Alert.BAD_MESSAGE_DATA` message if deserialization fails.
1.  Check that `version == 1`; send `Alert.BAD_VERSION` message if mismatch.
1.  Check that `random` is exactly 32 bytes; send `Alert.BAD_RANDOM` message if not.
1.  Check to see if any of the `handshake_cipher` in `cipher_commitment` are acceptable. Servers should select the first `handshake_cipher` that it finds acceptable to support clients signaling deprecated but supported HandshakeCiphers. If no `handshake_cipher` is acceptable (or there are no HandshakeCiphers in the message), the server sends an `Alert.BAD_HANDSHAKE_CIPHER` message.
1.  Checks that `next_protocol` contains a protocol that the server supports.  Send an `Alert.BAD_NEXT_PROTOCOL` message if not.

If no alerts have been sent, the server replies with the `ServerInit` message.


## The `ServerInit` Message 

The `ServerInit` message is as follows


```
message Ukey2ServerInit {
  optional int32 version = 1;  // highest supported version for rollback protection
  optional bytes random = 2;   // random bytes for replay/reuse protection

  // Selected Cipher and corresponding public key
  optional Ukey2HandshakeCipher handshake_cipher = 3;
  optional bytes public_key = 4;
}
```


For now, `version` must be 1.  The random field is exactly 32 cryptographically secure random
bytes.  The `handshake_cipher` field contains the server-chosen `HandshakeCipher`.  The
`public_key` field contains the server-chosen corresponding public key.


## Interpreting `ServerInit`

When a client receives a `ServerInit` after having sent a `ClientInit`, it performs the following actions:


1.  Deserialize the protobuf; send an `Alert.BAD_MESSAGE` message if deserialization fails.
1.  Verify that `message_type == Type.SERVER_INIT`; send an `Alert.BAD_MESSAGE_TYPE` message if mismatch occurs.
1.  Deserialize `message_data` as a `ServerInit` message; send an `Alert.BAD_MESSAGE_DATA` message if deserialization fails.
1.  Check that `version == 1`; send `Alert.BAD_VERSION` message if mismatch.
1.  Check that `random` is exactly 32 bytes; send `Alert.BAD_RANDOM` message if not.
1.  Check that `handshake_cipher` matches a handshake cipher that was sent in
`ClientInit.cipher_commitments`. If not, send an `Alert.BAD_HANDSHAKECIPHER` message.
1.  Check that `public_key` parses into a correct public key structure.  If not, send an `Alert.BAD_PUBLIC_KEY` message.

If no alerts have been sent, the client replies with the `ClientFinished` message.  After sending
the `ClientFinished` message, the Client considers the handshake complete.


**IMPORTANT:** The client should compute the authentication string `AUTH_STRING` and
the next-protocol secret `NEXT_SECRET` (see below).  The client should use an out-of-band
channel to verify the authentication string before proceeding to the next protocol.


## The ClientFinished Message 

The `ClientFinished` message is as follows:


```
message Ukey2ClientFinished {
  optional bytes public_key = 1;  // public key matching selected handshake cipher
}
```


The `public_key` contains the Client's public key (whose commitment was sent in the `ClientInit`
message) for the server-selected handshake cipher.


## Interpreting ClientFinished 

When a server receives a `ClientFinished` after having sent a `ServerInit`, it performs the
following actions:


1.  Deserialize the protobuf; terminate the connection if deserialization fails.
1.  Verify that `message_type == Type.CLIENT_FINISHED`; terminate the connection if mismatch occurs.
1.  Verify that the hash of the `ClientFinished` matches the expected commitment for the chosen `handshake_cipher` from `ClientInit`.  Terminate the connection if the expected match fails.
1.  Deserialize `message_data` as a `ClientFinished` message; terminate the connection if deserialization fails.
1.  Check that `public_key` parses into a correct public key structure.  If not, terminate the connection.

Note that because the client is not expecting a response, any error results in connection termination.

After parsing the `ClientFinished` message, the Server considers the handshake complete.


**IMPORTANT:** The server should compute the authentication string `AUTH_STRING` and the
next-protocol secret `NEXT_SECRET` (see below).  The server should use an out-of-band channel to
verify the authentication string before proceeding to the next protocol.


# Deriving the Authentication String and the Next-Protocol Secret

Let `DHS` = the negotiated Diffie-Hellman key derived from the Client and Server public keys.

Let `M_1` = the serialized (including framing) raw bytes of the first message sent by
the client

Let `M_2` = the serialized (including framing) raw bytes of the first message sent by
the server

Let `Hash` = the hash from HandshakeCipher

Let `L_auth` = length of authentication string in bytes.  Note that this length can
be short (e.g., a 6 digit visual confirmation code).

Let `L_next` = length of next protocol key

Let `HKDF-Extract` and `HKDF-Expand` be as defined in [RFC5869](https://tools.ietf.org/html/rfc5869)
instantiated with the hash from the `HandshakeCipher`.

Let `PRK_AUTH = HKDF-Extract("UKEY2 v1 auth", DHS)`

Let `PRK_NEXT = HKDF-Extract("UKEY2 v1 next", DHS)`

Then `AUTH_STRING = HKDF-Expand(PRK_AUTH, M_1|M_2, L_auth)`

Then `NEXT_SECRET = HKDF-Expand(PRK_NEXT, M_1|M_2, L_next)`


# Security Discussion

If client and server authenticate one-another using the `AUTH_STRING` through an out-of-band
mechanism, we believe that this handshake is resistant to an active man-in-the-middle attacker.
The attacker, whether he/she plays the role of the client or server, is forced to commit to a
public key before seeing the other-party's public key.

The authentication string and next secret are computed in such a way that knowledge of one does
not allow an attacker to compute the other.  That is, if the attacker observed the `AUTH_STRING`
(if it was shown on a monitor for example), the attacker could not compute `NEXT_SECRET`.
Furthermore, both the authentication string and next secret depend on the full handshake
transcript -- a manipulation of any handshake message by an adversary would change both the
 authentication string and the next secret.  Note that although the last message is not directly
 included in the HKDF computation, it is included as part of the commitment sent in `M_1.`

@shabsi pointed out that by having the `HKDF` info field have bits that also go into making the
`PRK`, this violates some security proof.  Those "shared" bits are the public keys that are sent
in `M_2` and `M_3` and are also used to derive the DHS.  Though the "proof" may
 not hold in theory, we do believe the security of the handshake is maintained in practice.

A natural question may be why we didn't use
[Short Authentication Strings](https://www.iacr.org/archive/crypto2005/36210303/36210303.pdf)
(SAS).  The answer is two-fold.  First, traditional SAS does not incorporate a key exchange, only
authentication; UKEY2 provides both.  Second, the paper does not give concrete primitives,
instead describing abstract functions such as `commit() `and `open()`.  One concrete
implementation of these functions would look similar to what UKEY2 does.

Bruno Blanchet performed a formal proof of a simplified version of UKEY2.

# Performance

The messages are fairly compact.  Running a test where the client sent a single commitment for a
`P256_SHA512` cipher and the `next_protocol` was set to "`AES_256_CBC-HMAC_SHA256"`, the total
size of the messages were:


| Message        | Length in Bytes |
|:---------------|----------------:|
|`ClientInit`    | 136             |
|`ServerInit`    | 117             |
|`ClientFinished`| 79              |


