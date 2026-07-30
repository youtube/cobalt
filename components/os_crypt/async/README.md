***OS Crypt Async***

This directory contains the new version of `OSCrypt` that supports asynchronous
initialization and pluggable providers. It has replaced the legacy synchronous
`OSCrypt` implementation which has now been removed.

**Main interfaces**

`browser/` should only be included by code that lives in the browser process. An
instance of `OSCryptAsync` should be constructed and held in browser and is
responsible for minting `Encryptor` instances. \/\/chrome holds a browser-wide
instance that's accessible from `g_browser_process` using `os_crypt_async()`
method.

`GetInstance` can be called as many times as necessary to obtain instances of
`Encryptor` that should be used for encryption operations. Note that all
`Encryptor` instances returned from the same instance of `OSCryptAsync` will
always be able to decrypt each other's data.

`common/` can be included by any code in any process and allows `Encryptor`
instances to perform encrypt/decrypt operations. These `EncryptString` and
`DecryptString` operations are sync and can be called on any thread.

`Encryptor` instances can be passed over mojo if necessary, as mojo traits exist
to serialize and deserialize. If an `Encryptor` instance is passed to a process
then that process will be able to decrypt any data encrypted with
`OSCryptAsync`.

It is preferred to use the `base::span` `EncryptData` and `DecryptData` APIs,
however the `EncryptString` and `DecryptString` APIs are provided for ease of
compatibility with existing callers. The string and span APIs are compatible
with one another.

`GetInstance()` must be called on the same sequence that it was created on,
which, if you are using the instance managed by \/\/chrome is the UI thread.
Therefore, plan for your `GetInstance` calls to be made on this sequence.
Callbacks will also arrive on this sequence, and note that the callback
might be executed before `GetInstance` returns, if the Encryptor is already
available. Once you have an `Encryptor` it can be safely passed and used on
another sequence, though.
