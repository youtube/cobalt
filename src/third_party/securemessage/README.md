# Summary
This is a **portable crypto library** that exposes a restricted API that is
**secure by design**, for use as a black-box building block in cryptographic
protocols.  Security and portability are emphasized over efficient (compact)
encodings, but keeping the crypto overhead reasonably minimal is still a
requirement. The API and implementation should encourage the use of best
practices, and reduce the likelihood of common protocol design errors.

# Overview
Existing cryptographic libraries available in most languages typically expose
raw crypto primitives (e.g., block ciphers with modes of operation, HMACs,
digital signatures) that are often applied incorrectly in protocol design.
While these libraries are "standards compliant", they do little or nothing to
guide the protocol designer to produce secure protocol designs. Worse still,
they leave ample room for implementations to introduce subtle security bugs
such as timing attacks.

The SecureMessage library leverages Google's protobuffer technology to provide
a simple and portable encoding for public keys and signed/encrypted data. The
exposed API consists of two main classes, and two supporting classes.

The main classes are:

* Secure Message Builder -- used to craft SecureMessage protos from the input
  data, metadata, and keys

* Secure Message Parser -- used to extract the original input data and metadata
  from a SecureMessage proto, while performing all necessary verification,
  decryption, and sanity checking in a secure fashion

The supporting classes are:

* PublicKeyProtoUtil -- provides simple protobuf based encoding/decoding for
  public keys (either EC or RSA), including appropriate security checks to
  reject maliciously crafted inputs (e.g., EC public keys that are not on the
  curve). Also assists with key generation.


* CryptoOps -- provides clean abstractions for various low-level cryptographic
  operations, making it easier to port the library to new crypto providers (and
  in the case of HKDF, providing clients of the library with access a portable
  API for that primitive too, since it is not commonly available).

In all cases, exposed APIs are carefully thought out, in order to encourage
safe use and prevent unsafe use. For example, the SecureMessageParser API will
not return the original input data unless the signature on it has been verified
first. It also requires the caller to specify exactly which signature algorithm
is expected to be used, to prevent callers from accidentally consuming messages
sent using any other (potentially deprecated / insecure) signature algorithms.

Currently supported signature algorithms are:

* EC DSA on the NIST P-256 curve, using a SHA-256 digest (ANSI X9.62)
* 2048-bit RSA signatures using a SHA-256 digest (PKCS#1)
* HMAC with SHA-256 for "symmetric key signature" (RFC 2104)

Currently supported encryption algorithms are:

* AES-256 in CBC mode for symmetric key encryption

# Features and API
The basic API for SecureMessage consists of two classes, one of which creates
SecureMessages, whereas the other parses them.  There are two basic types of
SecureMessage which these classes manipulate:

* Signed Cleartext - This is used for messages that do not require any privacy
  guarantee, but which need to be authenticated. In particular, the "signature"
  may be either a digital signature, or a MAC.

* Signcrypted - This is used when both privacy and authenticity are required.
  The body data will first be encrypted, and then "signed". (Special measures
  are taken to ensure that the original signature cannot be stripped off of the
  encrypted data and replaced by another signature from a different signer.)

Note that we use the term "signature" in a generic sense, to refer to either
digital signatures (using asymmetric keys) or Message Authentication Codes
(using symmetric keys). Currently, the encryption component of signcrypted
messages only supports symmetric key encryption schemes, but this may change in
the future.

Each SecureMessage consists of two primary components: a header and a body.
The header describes the structure of the SecureMessage as well as providing a
convenient location for metadata related to the body. The body of the
SecureMessage will contain the primary payload. Rather than using the protobuf
APIs to retrieve these components from a SecureMessage, the SecureMessageParser
must be used. The header component is always sent in the clear, and can be
extracted from the SecureMessage by calling SecureMessageParser's
getUnverifiedHeader() method without the use of any cryptographic keys. Thus,
it is possible to use the header contents to determine which key(s) must be
used to verify (and possibly decrypt) the body.  In order to retrieve the body
component, an appropriate call must be made to SecureMessageParser's
parseSignedCleartextMessage() or parseSigncryptedMessage() method, depending on
which of the two message types was sent.

To provide better support for building cryptographic protocols, we also provide
a utility class called PublicKeyProtoUtil that assists with encoding and
parsing public keys in a simple wire format, as well as generating new key
pairs. In particular, this class can be used to dramatically simplify
Diffie-Hellman key exchanges.

# Caveats
There is a some excess overhead incurred for using this library, as compared to
a more "direct" implementation of almost any of the offered functionality. The
excess overhead is the price being paid for simplicity and robustness, both in
the API and in the implementation. If saving every CPU cycle or every bit of
bandwidth is of crucial importance to you, do not use this library (for
example, it would not make for a good SSL replacement for high volume traffic).
To get a sense for magnitude of the SecureMessage overhead, a symmetric key
based HMAC SHA-256 and AES-256 CBC mode authenticated encryption incurs a
maximum of about 80 bytes of overhead with SecureMessage, as opposed to a
maximum of about 64 bytes of overhead using a direct implementation of the same
thing.

Encryption without signature is intentionally unsupported. Historically,
offering encryption-only APIs has lead to a plethora of mistakes in protocol
design and implementation, since it is frequently assumed that encrypted data
cannot be tampered with (but this is not the case in practice).

Support for public key encryption is not yet available, although it may be
added in the future. Instead, we recommend to design protocols that use
(EC)Diffie-Hellman key exchanges, and then used the exchanged symmetric key for
encryption. For instance, this approach can offer forward secrecy, and
increased efficiency when multiple messages are concerned.  Note that it is
easy to authenticate the Diffie-Hellman keys using a signature-only
SecureMessage wrapping with a public key signature scheme.

# Compilation and Use
See language specific implementation directories
