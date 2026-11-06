// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_TIMING_FAKE_H_
#define UI_GL_GPU_TIMING_FAKE_H_

#include <stdint.h>

#include <map>
#include <set>

#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

namespace gl {
class MockGLInterface;

class GPUTimingFake {
 public:
  GPUTimingFake();
  ~GPUTimingFake();

  void Reset();

  // Used to set the current GPU time queries will return.
  static int64_t GetFakeCPUTime(); // Useful for binding for Fake CPU time.
  void SetCurrentCPUTime(int64_t current_time);
  void SetCurrentGLTime(GLint64 current_time);
  void SetCPUGLOffset(int64_t offset);
#if BUILDFLAG(IS_STARBOARD)
  // Units per microsecond reported by glGetInteger64v(GL_TIMESTAMP_EXT).
  // Defaults to 1000 (nanoseconds); 24 models a raw 24 MHz counter.
  void SetGLTimestampUnitsPerMicrosecond(int64_t units_per_us);
#endif

  // Used to signal a disjoint occurred for disjoint timer queries.
  void SetDisjoint();

  // GPUTimer fake queries which can be called multiple times.
  void ExpectGetErrorCalls(MockGLInterface& gl);
  void ExpectDisjointCalls(MockGLInterface& gl);
  void ExpectNoDisjointCalls(MockGLInterface& gl);

  // GPUTimer fake queries which can only be called once per setup.
  void ExpectGPUTimeStampQuery(MockGLInterface& gl, bool elapsed_query);
  void ExpectGPUTimerQuery(MockGLInterface& gl, bool elapsed_query);
  void ExpectOffsetCalculationQuery(MockGLInterface& gl);
  void ExpectNoOffsetCalculationQuery(MockGLInterface& gl);

  // Fake GL Functions.
  void FakeGLGenQueries(GLsizei n, GLuint* ids);
  void FakeGLDeleteQueries(GLsizei n, const GLuint* ids);
  void FakeGLBeginQuery(GLenum target, GLuint id);
  void FakeGLEndQuery(GLenum target);
  void FakeGLGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params);
  void FakeGLQueryCounter(GLuint id, GLenum target);
  void FakeGLGetInteger64v(GLenum pname, GLint64 * data);
  void FakeGLGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
  void FakeGLGetIntegerv(GLenum pname, GLint* params);
  GLenum FakeGLGetError();

 protected:
  bool disjointed_ = false;
  static int64_t fake_cpu_time_;
  GLint64 current_gl_time_ = 0;
  int64_t gl_cpu_time_offset_ = 0;
#if BUILDFLAG(IS_STARBOARD)
  int64_t gl_timestamp_units_per_us_ = 1000;
#endif
  GLuint next_query_id_ = 0;
  std::set<GLuint> allocated_queries_;
  struct QueryResult {
    enum QueryResultType {
      kQueryResultType_Invalid,
      kQueryResultType_TimeStamp,
      kQueryResultType_Elapsed
    } type_ = kQueryResultType_Invalid;
    GLint64 begin_time_ = 0;
    GLint64 value_ = 0;
  };
  std::map<GLuint, QueryResult> query_results_;
  struct ElapsedQuery {
    bool active_ = false;
    GLuint query_id_ = 0;
    GLint64 begin_time_ = 0;

    void Reset() {
      active_ = false;
      query_id_ = 0;
      begin_time_ = 0;
    }
  };
  ElapsedQuery current_elapsed_query_;
};

}  // namespace gl

#endif  // UI_GL_GPU_TIMING_FAKE_H_
