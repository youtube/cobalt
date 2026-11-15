# Cobalt 27.lts source-level adblock (32-bit ARM)

Patch set that integrates a renderer-side ad blocker into
[`youtube/cobalt`](https://github.com/youtube/cobalt) for Android TV.

- **Target:** `youtube/cobalt` branch `27.lts` / tags `27.lts.10` and `27.lts.13`
  (both verified, see [Verification performed](#verification-performed))
- **Build:** `android-arm` (32-bit ARM)
- **APK:** `Cobalt.apk`, package `dev.cobalt.coat`
- **Layer:** renderer JavaScript injection (Layer 1)

The patch injects `cobalt/adblock/adblock.js` into every YouTube document at
`DidClearWindowObject()`, before the page's own scripts run. The script removes
ad entries from InnerTube JSON responses (`JSON.parse` hook and `fetch`
rewrite), so the leanback app never sees player/browse ad payloads.

## What this is, and what it is not

`youtube/cobalt` is the Cobalt engine, not the YouTube TV web app. The app is
loaded remotely from `https://www.youtube.com/tv`, so there is no local
leanback JavaScript to patch in this repository. This patch changes the engine
so that the remotely loaded app is modified at runtime inside the renderer.

It does **not**:

- bundle or modify YouTube's web assets;
- block ad network requests at the URL level (Layer 2), or rewrite response
  bodies in C++ (Layer 3);
- remove server-side stitched ads (SSAI) that are already muxed into the video
  stream;
- patch an already-installed Google-signed APK. This project builds Cobalt from
  source; see the companion `youtube-tv-cobalt-adblock` APK patcher for the
  smali/dex route.

## Requirements

- A Cobalt/Chromium build host: Linux x64, `depot_tools` in `PATH`, `gn`,
  `autoninja`, Android SDK/NDK, and the usual Cobalt build dependencies.
- A full `youtube/cobalt` checkout at `27.lts`, `27.lts.10`, or `27.lts.13`
  (large; use `gclient sync` as documented by Cobalt rather than a plain
  shallow clone).
- A 32-bit ARM Android TV device for installation. The device must support the
  `armeabi-v7a` ABI.

## Apply

From an existing Cobalt checkout created with depot_tools/gclient:

```bash
cd cobalt
git checkout 27.lts.10   # or 27.lts.13

/path/to/cobalt-adblock-27lts/apply.sh .
```

Check the result:

```bash
git status --short
# M cobalt/renderer/BUILD.gn
# M cobalt/renderer/cobalt_content_renderer_client.cc
# ?? cobalt/adblock/
```

The script refuses to apply when the patch does not apply cleanly and detects an
already-applied patch. To apply manually:

```bash
git apply --check /path/to/cobalt-adblock-27lts/patches/0001-cobalt-adblock-27lts.patch
git apply /path/to/cobalt-adblock-27lts/patches/0001-cobalt-adblock-27lts.patch
```

## Build (32-bit ARM)

```bash
/path/to/cobalt-adblock-27lts/build.sh .
```

Equivalent manual commands:

```bash
python3 cobalt/build/gn.py -p android-arm -c devel --no-rbe
autoninja -C out/android-arm_devel cobalt_apk
```

`gn.py` uses `-c` (`--build_type`), not `-t`. `--no-rbe` is used for a local
build; set `USE_RBE=1` if the host is configured for Cobalt's remote execution.
`gn.py` writes `out/android-arm_devel/args.gn`, which imports
`//cobalt/build/configs/android-arm/args.gn` (`target_cpu = "arm"`);
`cobalt_enable_adblock` defaults to `true`. The APK is written to:

```
out/android-arm_devel/apks/Cobalt.apk
```

Install:

```bash
adb uninstall dev.cobalt.coat  # removes app data; needed on signature conflict
adb install -r out/android-arm_devel/apks/Cobalt.apk
```

The source build is signed with the build's keystore, not Google's. An
`INSTALL_FAILED_UPDATE_INCOMPATIBLE` error means an existing Cobalt/YouTube
build with a different signature is installed; uninstalling it is required.

## Patch contents

| Path | Purpose |
| --- | --- |
| `cobalt/adblock/adblock.gni` | `cobalt_enable_adblock` GN arg (default `true`) |
| `cobalt/adblock/BUILD.gn` | `source_set("adblock")` |
| `cobalt/adblock/adblock.js` | ES5 payload: ad-key stripping and fetch/JSON hooks |
| `cobalt/adblock/adblock_js.h` | generated header embedding `adblock.js` |
| `cobalt/adblock/generate_adblock_js.py` | regenerates the header |
| `cobalt/adblock/adblock_render_frame_observer.{h,cc}` | `DidClearWindowObject()` injection |
| `cobalt/renderer/BUILD.gn` | adds the dependency and `COBALT_ENABLE_ADBLOCK` define |
| `cobalt/renderer/cobalt_content_renderer_client.cc` | creates the observer per render frame |

The observer checks `youtube.com` and `youtube-nocookie.com` with
`GURL::DomainIs()` and injects into each matching frame's main world, so the
page's own `JSON.parse` and `fetch` are hooked. The payload is guarded by
`window.__ytTvAdblock` and is idempotent.

## Verify without a device

Check that the checked-in header is not stale:

```bash
python3 tools/verify_embedded_payload.py /path/to/cobalt/cobalt/adblock
```

Check that the payload is compiled into the engine:

```bash
SO=$(find out/android-arm_devel -name libchrobalt.so -print -quit)
strings "$SO" | grep -m1 yt-tv-adblock
```

On device, `adb logcat` may show the payload's `[yt-tv-adblock] active` line.
Cobalt does not forward `console` on every build, so the line is best-effort.
The InnerTube key stripping is what does the work.

## Update the payload

The JavaScript is compiled into `libchrobalt.so`; there is no runtime asset to
swap.

```bash
$EDITOR cobalt/adblock/adblock.js
python3 cobalt/adblock/generate_adblock_js.py
python3 tools/verify_embedded_payload.py cobalt/adblock
autoninja -C out/android-arm_devel cobalt_apk
```

`cobalt/adblock/adblock.js` is the same logic as
`youtube-tv-cobalt-adblock/src/payload/adblock.js` (only the license/header
comment differs). The Node tests in that project cover the ad-key stripping,
browse feed, Shorts, and fetch rewrite behavior.

## Disable the feature

Option 1: revert the patch:

```bash
cd cobalt
git apply -R /path/to/cobalt-adblock-27lts/patches/0001-cobalt-adblock-27lts.patch
```

Option 2: keep the code but disable the observer. After `gn.py` has generated
the build directory, append the arg to `args.gn` (do not rerun `gn.py`
afterwards, since it overwrites `args.gn`):

```bash
echo 'cobalt_enable_adblock = false' >> out/android-arm_devel/args.gn
autoninja -C out/android-arm_devel cobalt_apk
```

The `//cobalt/adblock` target is still a dependency of the renderer, but the
`COBALT_ENABLE_ADBLOCK` define removes the observer instantiation.

## Compatibility and limitations

- The ad keys in the payload (`adPlacements`, `playerAds`, `adSlots`,
  `adBreakHeartbeatParams`, `importantAdBreak`, `adBreakServiceMetadata`) are
  based on the InnerTube responses used by the leanback client. If YouTube
  changes the schema, update `AD_KEYS` and the browse/Shorts filters.
- The patch targets the renderer layer. It cannot remove ads that are stitched
  into the media stream server-side.
- The hooks are installed in each document's main world. If the leanback client
  moves response parsing into a dedicated worker or service worker, the hooks
  must be installed in that context separately.
- XHR support covers asynchronous requests whose `responseType` is `""`,
  `"text"`, or `"json"`. Synchronous XHR and binary types (`blob`,
  `arraybuffer`, `document`) are left untouched.
- Browse and search promotions are removed alongside ad slots
  (`promotedSparkles*`, `searchPyvRenderer`, `mealbarPromoRenderer`,
  `backgroundPromoRenderer`, `compactPromoRenderer`, `statementBannerRenderer`,
  `inFeedAdLayoutRenderer`). If a surface should stay, drop its key from
  `PROMO_RENDERER_KEYS`.
- A full Cobalt build was not run in the environment that produced this patch
  set. Verify with `gn gen`/`autoninja` on a configured build host, then test on
  the target device.
- Modifying the YouTube client may violate YouTube's Terms of Service. Use this
  on devices you control; this repository does not redistribute Google code.

## Verification performed

- `git apply --check` against fresh copies of the
  `cobalt/renderer/BUILD.gn` and
  `cobalt/renderer/cobalt_content_renderer_client.cc` files fetched from both
  `27.lts.10` and `27.lts.13`: clean on both (`27.lts.10` applies with a
  one-line offset in hunk #2; the two files are not byte-identical across the
  tags, but the patched context matches).
- Reverse-apply check passes after applying.
- The embedded payload in `adblock_js.h` matches `adblock.js` byte-for-byte
  (ignoring the final newline), and both copies in this patch are byte-identical
  to the tested companion payload
  (`youtube-tv-cobalt-adblock/src/payload/adblock.js`); enforced by
  `tools/check_payload_sync.py`.
- Full Cobalt compilation is left to a build host with a complete checkout.

## Keeping the payload in sync

The canonical payload is
`youtube-tv-cobalt-adblock/src/payload/adblock.js`. The Cobalt patch embeds the
same bytes twice: as `cobalt/adblock/adblock.js` and inside the generated
`cobalt/adblock/adblock_js.h`. After editing the payload, update both sections
of `patches/0001-cobalt-adblock-27lts.patch` (or regenerate the header with
`cobalt/adblock/generate_adblock_js.py` in a checkout) and run:

```bash
python3 tools/check_payload_sync.py
```

The check exits non-zero when the copies drift or when `adblock_js.h` is stale.
