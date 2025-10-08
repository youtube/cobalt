#if defined(_WIN32) || defined(__CYGWIN__)
<<<<<<< HEAD
#include <windows.h>
#elif !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <pthread.h>
=======
	#include <windows.h>
#elif !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
	#include <pthread.h>
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
#endif

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#ifdef __APPLE__
<<<<<<< HEAD
#include "TargetConditionals.h"
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
static INIT_ONCE init_guard = INIT_ONCE_STATIC_INIT;
#elif !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
static pthread_once_t init_guard = PTHREAD_ONCE_INIT;
#else
static bool init_guard = false;
=======
	#include "TargetConditionals.h"
#endif


#if defined(_WIN32) || defined(__CYGWIN__)
	static INIT_ONCE init_guard = INIT_ONCE_STATIC_INIT;
#elif !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
	static pthread_once_t init_guard = PTHREAD_ONCE_INIT;
#else
	static bool init_guard = false;
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
#endif

bool CPUINFO_ABI cpuinfo_initialize(void) {
#if CPUINFO_ARCH_X86 || CPUINFO_ARCH_X86_64
<<<<<<< HEAD
#if defined(__MACH__) && defined(__APPLE__)
	pthread_once(&init_guard, &cpuinfo_x86_mach_init);
#elif defined(__FreeBSD__)
	pthread_once(&init_guard, &cpuinfo_x86_freebsd_init);
#elif defined(__linux__)
	pthread_once(&init_guard, &cpuinfo_x86_linux_init);
#elif defined(_WIN32) || defined(__CYGWIN__)
	InitOnceExecuteOnce(&init_guard, &cpuinfo_x86_windows_init, NULL, NULL);
#else
	cpuinfo_log_error("operating system is not supported in cpuinfo");
#endif
#elif CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
#if defined(__linux__)
	pthread_once(&init_guard, &cpuinfo_arm_linux_init);
#elif defined(__MACH__) && defined(__APPLE__)
	pthread_once(&init_guard, &cpuinfo_arm_mach_init);
#elif defined(_WIN32)
	InitOnceExecuteOnce(&init_guard, &cpuinfo_arm_windows_init, NULL, NULL);
#else
	cpuinfo_log_error("operating system is not supported in cpuinfo");
#endif
#elif CPUINFO_ARCH_RISCV32 || CPUINFO_ARCH_RISCV64
#if defined(__linux__)
	pthread_once(&init_guard, &cpuinfo_riscv_linux_init);
#else
	cpuinfo_log_error("operating system is not supported in cpuinfo");
#endif
#elif CPUINFO_ARCH_ASMJS || CPUINFO_ARCH_WASM || CPUINFO_ARCH_WASMSIMD
#if defined(__EMSCRIPTEN_PTHREADS__)
	pthread_once(&init_guard, &cpuinfo_emscripten_init);
#else
	if (!init_guard) {
		cpuinfo_emscripten_init();
	}
	init_guard = true;
#endif
=======
	#if defined(__MACH__) && defined(__APPLE__)
		pthread_once(&init_guard, &cpuinfo_x86_mach_init);
	#elif defined(__linux__)
		pthread_once(&init_guard, &cpuinfo_x86_linux_init);
	#elif defined(_WIN32) || defined(__CYGWIN__)
		InitOnceExecuteOnce(&init_guard, &cpuinfo_x86_windows_init, NULL, NULL);
	#else
		cpuinfo_log_error("operating system is not supported in cpuinfo");
	#endif
#elif CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
	#if defined(__linux__)
		pthread_once(&init_guard, &cpuinfo_arm_linux_init);
	#elif defined(__MACH__) && defined(__APPLE__)
		pthread_once(&init_guard, &cpuinfo_arm_mach_init);
	#elif defined(_WIN32)
		InitOnceExecuteOnce(&init_guard, &cpuinfo_arm_windows_init, NULL, NULL);
	#else
		cpuinfo_log_error("operating system is not supported in cpuinfo");
	#endif
#elif CPUINFO_ARCH_ASMJS || CPUINFO_ARCH_WASM || CPUINFO_ARCH_WASMSIMD
	#if defined(__EMSCRIPTEN_PTHREADS__)
		pthread_once(&init_guard, &cpuinfo_emscripten_init);
	#else
		if (!init_guard) {
			cpuinfo_emscripten_init();
		}
		init_guard = true;
	#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
#else
	cpuinfo_log_error("processor architecture is not supported in cpuinfo");
#endif
	return cpuinfo_is_initialized;
}

<<<<<<< HEAD
void CPUINFO_ABI cpuinfo_deinitialize(void) {}
=======
void CPUINFO_ABI cpuinfo_deinitialize(void) {
}
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
