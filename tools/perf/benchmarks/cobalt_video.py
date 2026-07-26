from benchmarks import perf_benchmark
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement
import page_sets.cobalt_video_playback

@benchmark.Info(emails=['your-email@google.com'], component='Cobalt')
class CobaltVideoBenchmark(perf_benchmark.PerfBenchmark):
    """Measures 5-minute 4K video playback performance on Cobalt."""
    
    def CreateStorySet(self, options):
        return page_sets.cobalt_video_playback.CobaltVideoStorySet()

    def CreateCoreTimelineBasedMeasurementOptions(self):
        category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()
        # Essential categories for Video/Memory profiling
        category_filter.AddIncludedCategory('cobalt')
        category_filter.AddIncludedCategory('media')
        category_filter.AddIncludedCategory('gpu')
        
        options = timeline_based_measurement.Options(category_filter)
        options.config.enable_chrome_trace = True
        return options

    @classmethod
    def Name(cls):
        return 'cobalt_video_perf'