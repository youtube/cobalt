/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef skiatest_Test_DEFINED
#define skiatest_Test_DEFINED

#include "include/core/SkString.h"
#include "include/core/SkTypes.h"
#include "src/core/SkTraceEvent.h"
#include "tools/Registry.h"
#include "tools/gpu/GrContextFactory.h"

namespace skgpu { class Context; }

namespace skiatest {

SkString GetTmpDir();

struct Failure {
    Failure(const char* f, int l, const char* c, const SkString& m)
        : fileName(f), lineNo(l), condition(c), message(m) {}
    const char* fileName;
    int lineNo;
    const char* condition;
    SkString message;
    SkString toString() const;
};

class Reporter : SkNoncopyable {
public:
    virtual ~Reporter() {}
    virtual void bumpTestCount();
    virtual void reportFailed(const skiatest::Failure&) = 0;
    virtual bool allowExtendedTest() const;
    virtual bool verbose() const;
    virtual void* stats() const { return nullptr; }

    void reportFailedWithContext(const skiatest::Failure&);

    void push(const SkString& message) {
        fContextStack.push_back(message);
    }
    void pop() {
        fContextStack.pop_back();
    }

private:
    SkTArray<SkString> fContextStack;
};

#define REPORT_FAILURE(reporter, cond, message) \
    reporter->reportFailedWithContext(skiatest::Failure(__FILE__, __LINE__, cond, message))

class ReporterContext : SkNoncopyable {
public:
    ReporterContext(Reporter* reporter, const SkString& message) : fReporter(reporter) {
        fReporter->push(message);
    }
    ~ReporterContext() {
        fReporter->pop();
    }

private:
    Reporter* fReporter;
};

typedef void (*TestProc)(skiatest::Reporter*, const GrContextOptions&);
typedef void (*ContextOptionsProc)(GrContextOptions*);

struct Test {
    Test(const char* name,
         bool needsGpu,
         bool needsGraphite,
         TestProc proc,
         ContextOptionsProc optionsProc = nullptr)
            : fName(name)
            , fNeedsGpu(needsGpu)
            , fNeedsGraphite(needsGraphite)
            , fProc(proc)
            , fContextOptionsProc(optionsProc) {}
    const char* fName;
    bool fNeedsGpu;
    bool fNeedsGraphite;
    TestProc fProc;
    ContextOptionsProc fContextOptionsProc;

    void modifyGrContextOptions(GrContextOptions* options) {
        if (fContextOptionsProc) {
            (*fContextOptionsProc)(options);
        }
    }

    void run(skiatest::Reporter* r, const GrContextOptions& options) const {
        TRACE_EVENT1("test", TRACE_FUNC, "name", this->fName/*these are static*/);
        this->fProc(r, options);
    }
};

typedef sk_tools::Registry<Test> TestRegistry;

/*
    Use the following macros to make use of the skiatest classes, e.g.

    #include "tests/Test.h"

    DEF_TEST(TestName, reporter) {
        ...
        REPORTER_ASSERT(reporter, x == 15);
        ...
        REPORTER_ASSERT(reporter, x == 15, "x should be 15");
        ...
        if (x != 15) {
            ERRORF(reporter, "x should be 15, but is %d", x);
            return;
        }
        ...
    }
*/

using GrContextFactoryContextType = sk_gpu_test::GrContextFactory::ContextType;

typedef void GrContextTestFn(Reporter*, const sk_gpu_test::ContextInfo&);
typedef bool GrContextTypeFilterFn(GrContextFactoryContextType);

extern bool IsGLContextType(GrContextFactoryContextType);
extern bool IsVulkanContextType(GrContextFactoryContextType);
extern bool IsMetalContextType(GrContextFactoryContextType);
extern bool IsDawnContextType(GrContextFactoryContextType);
extern bool IsDirect3DContextType(GrContextFactoryContextType);
extern bool IsRenderingGLContextType(GrContextFactoryContextType);
extern bool IsMockContextType(GrContextFactoryContextType);
void RunWithGPUTestContexts(GrContextTestFn*, GrContextTypeFilterFn*, Reporter*,
                            const GrContextOptions&);

namespace graphite {

typedef void GraphiteTestFn(Reporter*, skgpu::Context*);

void RunWithGraphiteTestContexts(GraphiteTestFn*, Reporter*);

} // namespace graphite

/** Timer provides wall-clock duration since its creation. */
class Timer {
public:
    /** Starts the timer. */
    Timer();

    /** Nanoseconds since creation. */
    double elapsedNs() const;

    /** Milliseconds since creation. */
    double elapsedMs() const;

    /** Milliseconds since creation as an integer.
        Behavior is undefined for durations longer than SK_MSecMax.
    */
    SkMSec elapsedMsInt() const;
private:
    double fStartNanos;
};

}  // namespace skiatest

#define REPORTER_ASSERT(r, cond, ...)                              \
    do {                                                           \
        if (!(cond)) {                                             \
            REPORT_FAILURE(r, #cond, SkStringPrintf(__VA_ARGS__)); \
        }                                                          \
    } while (0)

#define ERRORF(r, ...)                                      \
    do {                                                    \
        REPORT_FAILURE(r, "", SkStringPrintf(__VA_ARGS__)); \
    } while (0)

#define INFOF(REPORTER, ...)         \
    do {                             \
        if ((REPORTER)->verbose()) { \
            SkDebugf(__VA_ARGS__);   \
        }                            \
    } while (0)

#define DEF_TEST(name, reporter)                                                            \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);                  \
    skiatest::TestRegistry name##TestRegistry(                                              \
            skiatest::Test(#name, /*gpu*/ false, /*graphite*/ false, test_##name));         \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions&)

#define DEF_TEST_DISABLED(name, reporter) \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);                  \
    skiatest::TestRegistry name##TestRegistry(                                              \
            skiatest::Test(#name, /*gpu*/ false, /*graphite*/ false, test_##name));         \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions&) {               \
            /* SkDebugf("Disabled:"#name "\n"); */                                          \
    }                                                                                       \
    void disabled_##name(skiatest::Reporter* reporter, const GrContextOptions&)

#ifdef SK_BUILD_FOR_UNIX
    #define UNIX_ONLY_TEST DEF_TEST
#else
    #define UNIX_ONLY_TEST DEF_TEST_DISABLED
#endif

#define DEF_GRAPHITE_TEST(name, reporter)                                                   \
    static void test_##name(skiatest::Reporter*);                                           \
    static void test_graphite_##name(skiatest::Reporter* reporter,                          \
                                     const GrContextOptions& /*unused*/) {                  \
        test_##name(reporter);                                                              \
    }                                                                                       \
    skiatest::TestRegistry name##TestRegistry(                                              \
            skiatest::Test(#name, /*gpu*/ false, /*graphite*/ true, test_graphite_##name)); \
    void test_##name(skiatest::Reporter* reporter)

#define DEF_GRAPHITE_TEST_FOR_CONTEXTS(name, reporter, graphite_context)                    \
    static void test_##name(skiatest::Reporter*, skgpu::Context*);                          \
    static void test_graphite_contexts_##name(skiatest::Reporter* _reporter,                \
                                              const GrContextOptions& /*unused*/) {         \
        skiatest::graphite::RunWithGraphiteTestContexts(test_##name, _reporter);            \
    }                                                                                       \
    skiatest::TestRegistry name##TestRegistry(                                              \
            skiatest::Test(#name, /*gpu*/ false, /*graphite*/ true,                         \
                           test_graphite_contexts_##name));                                 \
    void test_##name(skiatest::Reporter* reporter, skgpu::Context* graphite_context)

#define DEF_GPUTEST(name, reporter, options)                                             \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);               \
    skiatest::TestRegistry name##TestRegistry(                                           \
            skiatest::Test(#name, /*gpu*/ true, /*graphite*/ false, test_##name));       \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions& options)

#define DEF_GPUTEST_FOR_CONTEXTS(name, context_filter, reporter, context_info, options_filter)  \
    static void test_##name(skiatest::Reporter*, const sk_gpu_test::ContextInfo&);              \
    static void test_gpu_contexts_##name(skiatest::Reporter* reporter,                          \
                                         const GrContextOptions& options) {                     \
        skiatest::RunWithGPUTestContexts(test_##name, context_filter, reporter, options);       \
    }                                                                                           \
    skiatest::TestRegistry name##TestRegistry(                                                  \
            skiatest::Test(#name, /*gpu*/ true, /*graphite*/ false, test_gpu_contexts_##name, options_filter));             \
    void test_##name(skiatest::Reporter* reporter, const sk_gpu_test::ContextInfo& context_info)

#define DEF_GPUTEST_FOR_ALL_CONTEXTS(name, reporter, context_info)                          \
        DEF_GPUTEST_FOR_CONTEXTS(name, nullptr, reporter, context_info, nullptr)

#define DEF_GPUTEST_FOR_RENDERING_CONTEXTS(name, reporter, context_info)                    \
        DEF_GPUTEST_FOR_CONTEXTS(name, sk_gpu_test::GrContextFactory::IsRenderingContext,   \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_ALL_GL_CONTEXTS(name, reporter, context_info)                       \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsGLContextType,                          \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(name, reporter, context_info)                 \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsRenderingGLContextType,                 \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_MOCK_CONTEXT(name, reporter, context_info)                          \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsMockContextType,                        \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_VULKAN_CONTEXT(name, reporter, context_info)                        \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsVulkanContextType,                      \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_METAL_CONTEXT(name, reporter, context_info)                         \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsMetalContextType,                       \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_D3D_CONTEXT(name, reporter, context_info)                           \
    DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsDirect3DContextType,                        \
                             reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_DAWN_CONTEXT(name, reporter, context_info)                          \
    DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsDawnContextType,                            \
                             reporter, context_info, nullptr)

#define REQUIRE_PDF_DOCUMENT(TEST_NAME, REPORTER)                          \
    do {                                                                   \
        SkNullWStream testStream;                                          \
        auto testDoc = SkPDF::MakeDocument(&testStream);                   \
        if (!testDoc) {                                                    \
            INFOF(REPORTER, "PDF disabled; %s test skipped.", #TEST_NAME); \
            return;                                                        \
        }                                                                  \
    } while (false)

#endif
