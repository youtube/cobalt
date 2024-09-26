# Largest Contentful Paint Changelog

This is a list of changes to [Largest Contentful Paint](https://web.dev/lcp).

* Chrome 112
  * Metric definition improvement: [Largest Contentful Paint ignores low-entropy images](2023_04_lcp.md)
* Chrome 111
  * Implementation optimizations: [Changes related to LCP, FCP and Paint Holding](2023_03_lcp_fcp.md)
* Chrome 99
  * Implementation optimizations: [Navigation optimizations and timeOrigin changes](2022_03_lcp_fcp.md)
* Chrome 98
  * Metric bug fix: [Text paints are more accurate](2021_11_lcp.md)
* Chrome 96
  * Metric bug fix: [Largest Contentful Paint uses the page viewport](2021_09_lcp.md)
* Chrome 88
  * Metric definition improvement: [Largest Contentful Paint ignores full viewport images](2020_11_lcp.md)
  * Metric definition improvement: [Largest Contentful Paint stops recording after input in an iframe](2020_11_lcp.md)
  * Metric definition improvement: [Largest Contentful Paint bug fix for some images with source changes](2020_11_lcp.md)
  * Metric definition improvement: [Largest Contentful Paint ignores removed content by default](2020_11_lcp_2.md)
* Chrome 86
  * Metric definition improvement: [Largest Contentful Paint ignores paints with opacity 0](2020_08_lcp.md)
* Chrome 83
  * Metric definition improvement: [Largest Contentful Paint measurement stops at first input or scroll](2020_05_lcp.md)
  * Metric definition improvement: [Largest Contentful Paint properly accounts for visual size of background images](2020_05_lcp.md)
* Chrome 81
  * Metric definition improvement: [Largest Text Paint correctly reported while largest image is loading](2020_04_lcp.md)
* Chrome 79
  * Metric is elevated to stable; changes in metric definition will be reported in this log.
* Chrome 77
  * Experimental metric exposed via API: [Largest Contentful Paint](https://web.dev/lcp/) available via [PerformanceObserver API](https://wicg.github.io/largest-contentful-paint/)
