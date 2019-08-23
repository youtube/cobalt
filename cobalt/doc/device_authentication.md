# Device Authentication

Starting in Cobalt 20, initial URL requests will now have query parameters
appended to them signed by the platform's secret key.  The key is provided during
the certification process.  The key must be stored in secure storage on the
device.

## Message

When constructing the URL for the initial browse request, according to the
logic in
[cobalt/browser/device_authentication.cc](../browser/device_authentication.cc),
it will fetch from the platform a "certification scope" string provided to
the device during certification.  The certification scope will be queried
by a call to `SbSystemGetProperty(kSbSystemPropertyCertificationScope, ...)`,
which the platform is expected to implement.  Along with the current system
time, this forms the message that must be signed by the device's secret key.

## Signing

The message defined above must be signed with the HMAC-SHA256 algorithm. The
resulting digest (encoded as base64), alongside the unencrypted message
contents, will be appended to the initial URL.

Two choices exists for how platforms can expose the secret key to Cobalt.
Cobalt will first attempt to have the platform sign the message, and if that
functionality is not implemented Cobalt will query the platform for the secret
key and sign the message itself.  If neither choice is implemented, then Cobalt
will log a warning and not append anything to the URL.

### Platform signing

Cobalt will first attempt to use the `SbSystemSignWithCertificationSecretKey()`
function to sign the message using the secret key.  This method is preferred
since it enables implementations where the key exists only in secure hardware
and never enters the system's main memory.  A reference implementation, which
depends on BoringSSL exists at
[starboard/linux/x64x11/internal/system_sign_with_certification_secret_key.cc](../../starboard/linux/x64x11/internal/system_sign_with_certification_secret_key.cc).

### Cobalt signing

If the function `SbSystemSignWithCertificationSecretKey()` is unimplemented (e.g. it returns `false`, as is done in
[starboard/shared/stub/system_sign_with_certification_secret_key.cc](../../starboard/shared/stub/system_sign_with_certification_secret_key.cc)),
then Cobalt will instead attempt to retrieve the secret key from the system by
a call to
`SbSystemGetProperty(kSbSystemPropertyBase64EncodedCertificationSecret, ...)`,
and use it to produce the HMAC-SHA256 digest of the message itself.
