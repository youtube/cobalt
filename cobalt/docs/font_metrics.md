# Font Resource and Memory Usage Metrics in Cobalt

This document summarizes the UMA histograms, Memory Infra dumps, and performance metrics used to track font, glyph cache, and text rendering resource usage in Cobalt.

## 1. Memory Consumption (Memory Infra / UMA)

These metrics track actual bytes used in RAM and GPU memory.

### Skia Graphics Layer
*   **Skia Glyph Cache**
    *   **Dump Path**: `skia/sk_glyph_cache`
    *   **UMA Mapping**: `Memory.Experimental.[Process].Skia.SkGlyphCache`
    *   **Description**: Tracks the memory used by Skia's internal "Strike" cache (rasterized glyph masks).
    *   **Source**: `skia/ext/skia_memory_dump_provider.cc` via `SkGraphics::GetFontCacheUsed()`.
    *   **Cobalt Status**: Enabled in `cobalt/tools/uma/memory_histograms.txt`.

*   **GPU Glyph Atlas**
    *   **Dump Path**: `skia/gpu_resources/glyph_atlas` (in Chromium/Skia)
    *   **UMA Mapping**: `Memory.Gpu.Skia.GpuResources.GlyphAtlas`
    *   **Description**: Memory used by the GPU texture atlas where glyphs are packed for hardware-accelerated rendering.

### Blink / Renderer Layer
*   **Font Shape Caches**
    *   **Dump Path**: `font_caches/shape_caches`
    *   **UMA Mapping**: Part of `Memory.Experimental.Renderer2.FontCaches` (aggregated in some versions).
    *   **Description**: Memory used for caching text shaping results (the output of HarfBuzz).
    *   **Source**: `third_party/blink/renderer/platform/fonts/font_cache.cc`.

*   **Font Platform Data Cache**
    *   **Dump Path**: `font_caches/font_platform_data_cache`
    *   **Description**: Tracks memory used by the cache of platform-specific font handles.
    *   **Allowlist**: Included in `base/trace_event/memory_infra_background_allowlist.cc`.

## 2. Resource Efficiency & Cache Performance

*   **RenderTextHarfBuzz.ShapeRunsFallback**
    *   **Type**: Enumeration
    *   **Description**: Tracks how often font fallback is triggered during the text shaping phase.
    *   **Source**: `ui/gfx/render_text_harfbuzz.cc`.

*   **RenderTextHarfBuzz.GetFallbackFontTime**
    *   **Type**: Times (ms)
    *   **Description**: Time spent searching for a suitable fallback font when a character is missing.
    *   **Source**: `ui/gfx/render_text_harfbuzz.cc`.

*   **Blink.Fonts.DecodeTime**
    *   **Type**: Microseconds
    *   **Description**: Time spent decoding font resources.
    *   **Source**: `third_party/blink/renderer/platform/fonts/font_resource.cc`.

## 3. Cobalt Integration Details

*   **Metrics Management**: `cobalt/browser/metrics/cobalt_metrics_service_client.cc` coordinates global memory dumps.
*   **Allowlist**: Cobalt-specific histogram reporting is controlled by `cobalt/tools/uma/memory_histograms.txt`.

## How to Verify in Cobalt

1.  **Memory Dumps**: Trigger a global memory dump via CDP or the internal metrics service to see the `skia/sk_glyph_cache` and `font_caches/*` breakdown.
2.  **UMA Verification**: Use `cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py` to fetch currently recorded histograms from a running Cobalt instance and verify font-related entries.
3.  **Unit Tests**: Exercise `ui/gfx/render_text_unittest.cc` or similar tests to see histogram emission in a controlled environment.
