#include <benchmark/benchmark.h>

#include <cpuinfo.h>

<<<<<<< HEAD
=======

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
static void cpuinfo_initialize(benchmark::State& state) {
	while (state.KeepRunning()) {
		cpuinfo_initialize();
	}
}
BENCHMARK(cpuinfo_initialize)->Iterations(1)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
