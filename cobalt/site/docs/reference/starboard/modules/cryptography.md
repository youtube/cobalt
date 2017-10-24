---
layout: doc
title: "Starboard Module Reference: cryptography.h"
---

Hardware-accelerated cryptography. Platforms should **only** implement this
when there are **hardware-accelerated** cryptography facilities. Applications
must fall back to platform-independent CPU-based algorithms if the cipher
algorithm isn't supported in hardware.<br>
# Tips for Porters<br>
You should implement cipher algorithms in this descending order of priority
to maximize usage for SSL.<br>
<ol><li>GCM - The preferred block cipher mode for OpenSSL, mainly due to speed.
</li><li>CTR - This can be used internally with GCM, as long as the CTR
implementation only uses the last 4 bytes of the IV for the
counter. (i.e. 96-bit IV, 32-bit counter)
</li><li>ECB - This can be used (with a null IV) with any of the other cipher
block modes to accelerate the core AES algorithm if none of the
streaming modes can be accelerated.
</li><li>CBC - GCM is always preferred if the server and client both support
it. If not, they will generally negotiate down to AES-CBC. If this
happens, and CBC is supported by SbCryptography, then it will be
accelerated appropriately. But, most servers should support GCM,
so it is not likely to come up much, which is why it is the lowest
priority.</li></ol>
Further reading on block cipher modes:
https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation
https://crbug.com/442572
https://crypto.stackexchange.com/questions/10775/practical-disadvantages-of-gcm-mode-encryption

## Enums

### SbCryptographyBlockCipherMode

The method of chaining encrypted blocks in a sequence.
https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation

**Values**

*   `kSbCryptographyBlockCipherModeCbc` - Cipher Block Chaining mode.
*   `kSbCryptographyBlockCipherModeCfb` - Cipher Feedback mode.
*   `kSbCryptographyBlockCipherModeCtr` - Counter mode: A nonce is combined with an increasing counter.
*   `kSbCryptographyBlockCipherModeEcb` - Electronic Code Book mode: No chaining.
*   `kSbCryptographyBlockCipherModeOfb` - Output Feedback mode.
*   `kSbCryptographyBlockCipherModeGcm` - Galois/Counter Mode.

### SbCryptographyDirection

The direction of a cryptographic transformation.

**Values**

*   `kSbCryptographyDirectionEncode` - Cryptographic transformations that encode/encrypt data into atarget format.
*   `kSbCryptographyDirectionDecode` - Cryptographic transformations that decode/decrypt data intoits original form.

## Functions

### SbCryptographyCreateTransformer

**Description**

Creates an <code><a href="#sbcryptographytransform">SbCryptographyTransform</a></code>er with the given initialization
data. It can then be used to transform a series of data blocks.
Returns kSbCryptographyInvalidTransformer if the algorithm isn't
supported, or if the parameters are not compatible with the
algorithm.<br>
An <code><a href="#sbcryptographytransform">SbCryptographyTransform</a></code>er contains all state to start decrypting
a sequence of cipher blocks according to the cipher block mode. It
is not thread-safe, but implementations must allow different
SbCryptographyTransformer instances to operate on different threads.<br>
All parameters must not be assumed to live longer than the call to
this function. They must be copied by the implementation to be
retained.<br>
This function determines success mainly based on whether the combination of
`algorithm`, `direction`, `block_size_bits`, and `mode` is supported and
whether all the sizes passed in are sufficient for the selected
parameters. In particular, this function cannot verify that the key and IV
used were correct for the ciphertext, were it to be used in the decode
direction. The caller must make that verification.<br>
For example, to decrypt AES-128-CTR:
SbCryptographyCreateTransformer(kSbCryptographyAlgorithmAes, 128,
kSbCryptographyDirectionDecode,
kSbCryptographyBlockCipherModeCtr,
...);

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographyCreateTransformer-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographyCreateTransformer-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographyCreateTransformer-declaration">
<pre>
SB_EXPORT SbCryptographyTransformer
SbCryptographyCreateTransformer(const char* algorithm,
                                int block_size_bits,
                                SbCryptographyDirection direction,
                                SbCryptographyBlockCipherMode mode,
                                const void* initialization_vector,
                                int initialization_vector_size,
                                const void* key,
                                int key_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographyCreateTransformer-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

SbCryptographyTransformer SbCryptographyCreateTransformer(
    const char* algorithm,
    int block_size_bits,
    SbCryptographyDirection direction,
    SbCryptographyBlockCipherMode mode,
    const void* initialization_vector,
    int initialization_vector_size,
    const void* key,
    int key_size) {
  SB_UNREFERENCED_PARAMETER(algorithm);
  SB_UNREFERENCED_PARAMETER(block_size_bits);
  SB_UNREFERENCED_PARAMETER(direction);
  SB_UNREFERENCED_PARAMETER(mode);
  SB_UNREFERENCED_PARAMETER(initialization_vector);
  SB_UNREFERENCED_PARAMETER(initialization_vector_size);
  SB_UNREFERENCED_PARAMETER(key);
  SB_UNREFERENCED_PARAMETER(key_size);
  return kSbCryptographyInvalidTransformer;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>algorithm</code></td>
    <td>A string that represents the cipher algorithm.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>block_size_bits</code></td>
    <td>The block size variant of the algorithm to use, in bits.</td>
  </tr>
  <tr>
    <td><code>SbCryptographyDirection</code><br>        <code>direction</code></td>
    <td>The direction in which to transform the data.</td>
  </tr>
  <tr>
    <td><code>SbCryptographyBlockCipherMode</code><br>        <code>mode</code></td>
    <td>The block cipher mode to use.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>initialization_vector</code></td>
    <td>The Initialization Vector (IV) to use. May be NULL
for block cipher modes that don't use it, or don't set it at init time.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>initialization_vector_size</code></td>
    <td>The size, in bytes, of the IV.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>key</code></td>
    <td>The key to use for this transformation.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>key_size</code></td>
    <td>The size, in bytes, of the key.</td>
  </tr>
</table>

### SbCryptographyDestroyTransformer

**Description**

Destroys the given `transformer` instance.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographyDestroyTransformer-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographyDestroyTransformer-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographyDestroyTransformer-declaration">
<pre>
SB_EXPORT void SbCryptographyDestroyTransformer(
    SbCryptographyTransformer transformer);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographyDestroyTransformer-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

void SbCryptographyDestroyTransformer(SbCryptographyTransformer transformer) {
  SB_UNREFERENCED_PARAMETER(transformer);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>
        <code>transformer</code></td>
    <td> </td>
  </tr>
</table>

### SbCryptographyGetTag

**Description**

Calculates the authenticator tag for a transformer and places up to
`out_tag_size` bytes of it in `out_tag`. Returns whether it was able to get
the tag, which mainly has to do with whether it is compatible with the
current block cipher mode.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographyGetTag-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographyGetTag-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographyGetTag-declaration">
<pre>
SB_EXPORT bool SbCryptographyGetTag(
    SbCryptographyTransformer transformer,
    void* out_tag,
    int out_tag_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographyGetTag-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

bool SbCryptographyGetTag(
    SbCryptographyTransformer transformer,
    void* out_tag,
    int out_tag_size) {
  SB_UNREFERENCED_PARAMETER(transformer);
  SB_UNREFERENCED_PARAMETER(out_tag);
  SB_UNREFERENCED_PARAMETER(out_tag_size);
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>
        <code>transformer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>out_tag</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>out_tag_size</code></td>
    <td> </td>
  </tr>
</table>

### SbCryptographyIsTransformerValid

**Description**

Returns whether the given transformer handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbCryptographyIsTransformerValid(
    SbCryptographyTransformer transformer) {
  return transformer != kSbCryptographyInvalidTransformer;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>
        <code>transformer</code></td>
    <td> </td>
  </tr>
</table>

### SbCryptographySetAuthenticatedData

**Description**

Sets additional authenticated data (AAD) for a transformer, for chaining
modes that support it (GCM). Returns whether the data was successfully
set. This can fail if the chaining mode doesn't support AAD, if the
parameters are invalid, or if the internal state is invalid for setting AAD.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographySetAuthenticatedData-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographySetAuthenticatedData-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographySetAuthenticatedData-declaration">
<pre>
SB_EXPORT bool SbCryptographySetAuthenticatedData(
    SbCryptographyTransformer transformer,
    const void* data,
    int data_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographySetAuthenticatedData-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

bool SbCryptographySetAuthenticatedData(
    SbCryptographyTransformer transformer,
    const void* data,
    int data_size) {
  SB_UNREFERENCED_PARAMETER(transformer);
  SB_UNREFERENCED_PARAMETER(data);
  SB_UNREFERENCED_PARAMETER(data_size);
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>
        <code>transformer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>data</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>data_size</code></td>
    <td> </td>
  </tr>
</table>

### SbCryptographySetInitializationVector

**Description**

Sets the initialization vector (IV) for a transformer, replacing the
internally-set IV. The block cipher mode algorithm will update the IV
appropriately after every block, so this is not necessary unless the stream
is discontiguous in some way. This happens with AES-GCM in TLS.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographySetInitializationVector-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographySetInitializationVector-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographySetInitializationVector-declaration">
<pre>
SB_EXPORT void SbCryptographySetInitializationVector(
    SbCryptographyTransformer transformer,
    const void* initialization_vector,
    int initialization_vector_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographySetInitializationVector-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

void SbCryptographySetInitializationVector(
    SbCryptographyTransformer transformer,
    const void* initialization_vector,
    int initialization_vector_size) {
  SB_UNREFERENCED_PARAMETER(transformer);
  SB_UNREFERENCED_PARAMETER(initialization_vector);
  SB_UNREFERENCED_PARAMETER(initialization_vector_size);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>
        <code>transformer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>initialization_vector</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>initialization_vector_size</code></td>
    <td> </td>
  </tr>
</table>

### SbCryptographyTransform

**Description**

Transforms one or more `block_size_bits`-sized blocks of `in_data`, with the
given `transformer`, placing the result in `out_data`. Returns the number of
bytes that were written to `out_data`, unless there was an error, in which
case it will return a negative number.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCryptographyTransform-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCryptographyTransform-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCryptographyTransform-declaration">
<pre>
SB_EXPORT int SbCryptographyTransform(
    SbCryptographyTransformer transformer,
    const void* in_data,
    int in_data_size,
    void* out_data);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCryptographyTransform-stub">

```
#include "starboard/configuration.h"
#include "starboard/cryptography.h"

int SbCryptographyTransform(SbCryptographyTransformer transformer,
                            const void* in_data,
                            int in_data_size,
                            void* out_data) {
  SB_UNREFERENCED_PARAMETER(transformer);
  SB_UNREFERENCED_PARAMETER(in_data);
  SB_UNREFERENCED_PARAMETER(in_data_size);
  SB_UNREFERENCED_PARAMETER(out_data);
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbCryptographyTransformer</code><br>        <code>transformer</code></td>
    <td>A transformer initialized with an algorithm, IV, cipherkey,
and so on.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>in_data</code></td>
    <td>The data to be transformed.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>in_data_size</code></td>
    <td>The size of the data to be transformed, in bytes. Must be a
multiple of the transformer's <code>block-size_bits</code>, or an error will be
returned.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>out_data</code></td>
    <td>A buffer where the transformed data should be placed. Must have
at least capacity for <code>in_data_size</code> bytes. May point to the same memory as
<code>in_data</code>.</td>
  </tr>
</table>

