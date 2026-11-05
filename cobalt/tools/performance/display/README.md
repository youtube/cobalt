# Display ground truth on RDK: DRM CRTC CRC

`rdk_display_crc.py` measures what the **panel actually shows** on an RDK
(Amlogic) device, independently of Cobalt, Chromium, Westeros or any
software timer. It is the reference we use to validate frame-cadence metrics
such as the YTS "In-app Cert Performance" `cobalt-frame` `PresentIntervalMs`
buckets, and to tell apart "the app rendered a frame" from "the viewer saw a
new frame".

## How it works

The DRM core exposes a per-CRTC CRC interface in debugfs:

```
/sys/kernel/debug/dri/0/crtc-0/crc/control   # select CRC source
/sys/kernel/debug/dri/0/crtc-0/crc/data      # one line per VBlank
```

On Amlogic (e.g. AH212 / `meson` DRM) writing `vpp1` to `control` makes the
Video Post Processor compute a CRC over the **final, post-blend** pixels of
every output frame - i.e. the composite of the Cobalt graphics plane, the
video plane and anything else, exactly what is sent to HDMI. Reading `data`
blocks until the next VBlank and returns:

```
<vblank sequence (hex)> <crc (hex)>
0x010fd719 0x570affcc
```

That gives two facts per VBlank:

| Field | Meaning |
| :--- | :--- |
| VBlank sequence | Monotonic counter, +1 every 16.683 ms at 59.94 Hz. Gaps mean the reader fell behind, not that the display skipped. |
| CRC | Content fingerprint of the output image. **Changed vs previous line = a new image reached the panel. Identical = the previous image was repeated (frame held).** |

So the number of VBlanks a CRC is held for is the true present interval of
that frame in VSync units, and `1 / (mean hold)` is the on-screen frame rate.

Properties worth knowing:

* Zero-cost and non-intrusive: enabling the CRC source does not change the
  displayed image or timing.
* Only one process can hold `crc/data` open (second reader gets `EBUSY`).
  The tool resets `control` (`NULL` then `vpp1`) on start and exit.
* Panel cadence on the devices measured so far is 59.9401 Hz
  (16.683 ms), not 60.000 Hz. Use `--vsync-ms` for other modes.
* `cat`/BusyBox `awk` on the device work too; the Python tool just adds
  decoding, cadence statistics and offline analysis.

## Usage

```bash
# Live: one line per displayed frame; Ctrl-C prints the cadence summary.
cobalt/tools/performance/display/rdk_display_crc.py --device-ip <ip>

# Capture 30 s to a file (raw crc/data lines) and summarise.
cobalt/tools/performance/display/rdk_display_crc.py --device-ip <ip> \
    --duration 30 --output /tmp/crc.txt

# Print every VBlank, including repeats.
cobalt/tools/performance/display/rdk_display_crc.py --device-ip <ip> --raw

# Offline analysis of a saved capture; treat holds >= 30 VSyncs (0.5 s) as
# idle so they do not count as hitches.
cobalt/tools/performance/display/rdk_display_crc.py --input /tmp/crc.txt \
    --idle-vsyncs 30
```

The summary reports VBlanks sampled, new images on panel (on-screen fps),
hold-length histogram, VSync buckets using the same half-step boundaries as
the YTS gate (`<25 ms = 1V`, `<41.7 = 2V`, `<58.4 = 3V`, else `4+`), hitch
rate (`>1 VSync`), p50/p95 present interval and a best-effort cadence label
(60 fps, 30 fps, 24 fps 3:2 pulldown).

## Evidence (raw readings from an AH212 at 2160p60)

### Idle home screen - same CRC every VBlank

```
0x010f9a78 0x3a355ace
0x010f9a79 0x3a355ace
0x010f9a7a 0x3a355ace
...
0x010f9a81 0x3a355ace
```

Ten consecutive VBlanks, one CRC: nothing was redrawn. `--duration 10` on an
idle device reports `466 vblanks sampled, 0 new images on panel`.

### UI navigation (D-pad scrolling the shelf)

```
0x010fd719 0x570affcc   <- new image
0x010fd71a 0x15101ab8   <- new image, held 2 VSyncs
0x010fd71b 0x15101ab8
0x010fd71c 0xf8f809b2   <- new image, held 3 VSyncs
0x010fd71d 0xf8f809b2
0x010fd71e 0xf8f809b2
0x010fd71f 0x5d9368c8   <- new image, held 2 VSyncs
0x010fd720 0x5d9368c8
0x010fd721 0xeec8153b   <- new image
0x010fd722 0xeec8153b
```

The hold lengths (1, 2, 3, 2, ...) are exactly the per-frame present
intervals a frame-time metric should report for this animation.

### Inline video playback - 24 fps content, 3:2 pulldown

```
0x010fde0f 0x2054cb6d   held 3 VSyncs (50.0 ms)
0x010fde12 0xbd3330c5   held 2 VSyncs (33.4 ms)
0x010fde14 0x93ef8c86   held 3
0x010fde17 0x6f1dbbf5   held 2
0x010fde19 0x1b13fb56   held 2
0x010fde1b 0xea5030c9   held 3
0x010fde1e 0xbcce3a7e   held 2
0x010fde20 0xe37c4181   held 3
0x010fde23 0x405fa7f7   held 2
0x010fde25 0x466503c3   held 2
0x010fde27 0x57001d58   held 3
0x010fde2a 0x95f0451e   held 2
```

```
==== display cadence summary ====
vblanks sampled     : 30  (seq span 30, ~0.5 s at 59.94 Hz)
new images on panel : 12  (avg 24.0 fps on screen)
hold length (vsyncs) : 2x6, 3x5
VSync buckets        : 1V=  0.0%  2V= 54.5%  3V= 45.5%  4+V=  0.0%
cadence pattern      : 24 fps via 3:2 pulldown (2,3,2,3... hold pattern)
```

The 3-2-3-2-2-3 hold pattern is textbook 3:2 pulldown of 24 fps film onto a
60 Hz panel (2.5 VSyncs per frame on average). This also shows why a
whole-run CRC histogram is dominated by 2V/3V holds whenever video is on
screen: segment by activity (UI navigation vs playback) before comparing
against a UI frame-time metric.

### Using it to validate `cobalt-frame` (YTS In-app Cert Performance)

Captured concurrently with a full YTS run and segmented into the same
key-press to last-frame navigation windows the test uses:

| Phase | Source | p95 interval | 1 VSync | 2 VSync | 4+ VSync | Hitch rate |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| Tile-to-tile | display CRC | 45 ms | 90.4 % | 4.5 % | 2.1 % | 9.6 % |
| Tile-to-tile | `cobalt-frame` | 50.1 ms | 76.1 % | 15.4 % | 4.2 % | 23.9 % |
| Row-to-row | display CRC | 50 ms | 88.9 % | 5.7 % | 2.4 % | 11.1 % |
| Row-to-row | `cobalt-frame` | 61.0 ms | 75.2 % | 14.7 % | 6.2 % | 24.8 % |

91 % of `cobalt-frame` entries had a CRC change within +/-8 ms, so the
metric does track real frames, but its intervals are timestamped from the
GL fence completion on the CPU rather than from the VBlank. That produced
sub-VSync intervals (`<15 ms`, which the display can never show) followed by
compensating 2-VSync intervals, inflating the reported hitch rate by about
2.5x. The CRC capture is what made that bias visible and quantifiable; see
`ui/gl/gl_surface_presentation_helper.cc` for the Starboard-side fix.

## Caveats

* Requires debugfs (`/sys/kernel/debug`) and root; the CRC source name
  (`vpp1`) is Amlogic-specific. Other SoCs expose `auto` or vendor names in
  `crc/control`.
* A CRC change says "pixels changed", not "the intended frame was shown".
  Combine with the app-side frame timeline (e.g. YTS `certPerfDiagnostics`)
  when per-frame attribution is needed.
* Reads are blocking; if the reader stalls the kernel buffer overruns and
  the sequence counter jumps (reported as `non-consecutive vblank sequence
  gaps`). Avoid heavy per-line processing on the device.
