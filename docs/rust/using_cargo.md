# Using cargo

If you're building a tool that's meant to integrate with Chromium, you should
[use our build system](/docs/rust/README.md#first-party-crates)
(`gn` and `ninja`)

However, if you are building a throwaway or experimental tool, you might like to
use pure `cargo` tooling instead. Even then, you may choose
to restrict yourself to the toolchain and crates that are already approved for
use in Chromium, by:

* Using `tools/crates/run_cargo.py` (which will use
  Chromium's `//third_party/rust-toolchain/bin/cargo`)
* Configuring `.cargo/config.toml` to ask to use the crates vendored
  into Chromium's `//third_party/rust/chromium_crates_io`.

An example of how this works can be found in
<https://crrev.com/c/6320795/5>.
