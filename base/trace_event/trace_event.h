// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "base/trace_event/ should be reverted to use perfetto."
#endif

#define TRACE_EVENT0(category_group, name) ""
#define TRACE_EVENT1(category_group, name, arg1_name, arg1_val) ""
#define TRACE_EVENT2(category_group, name, arg1_name, arg1_val, arg2_name,   \
                     arg2_val) ""

#define INTERNAL_TRACE_EVENT_ADD(...) (void)0

#define TRACE_EVENT_BEGIN0(category_group, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_BEGIN1(category_group, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category_group, name, arg1_name, arg1_val,     \
                           arg2_name, arg2_val)                           \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,    \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_BEGIN0(category_group, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN2(category_group, name, arg1_name, arg1_val, \
                                arg2_name, arg2_val)                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val,     \
                           arg2_name, arg2_val)

#define TRACE_EVENT_END0(category_group, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_END1(category_group, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category_group, name, arg1_name, arg1_val, arg2_name, \
                         arg2_val)                                             \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,        \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,         \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_END0(category_group, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END2(category_group, name, arg1_name, arg1_val, \
                              arg2_name, arg2_val)                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val,   \
                           arg2_name, arg2_val)

#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category_group, name, id, \
                                         flags, ...)                      (void)0

#define TRACE_EVENT_ASYNC_BEGIN0(category_group, name, id) (void)0
#define TRACE_EVENT_ASYNC_BEGIN1(category_group, name, id, arg1_name, \
                                 arg1_val) (void)0
#define TRACE_EVENT_ASYNC_BEGIN2(category_group, name, id, arg1_name, \
                                 arg1_val, arg2_name, arg2_val) (void)0
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category_group, name, id) (void)0
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category_group, name, id, arg1_name, \
                                      arg1_val) (void)0
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category_group, name, id, arg1_name, \
                                      arg1_val, arg2_name, arg2_val) (void)0

#define TRACE_EVENT_ASYNC_END0(category_group, name, id) (void)0
#define TRACE_EVENT_ASYNC_END1(category_group, name, id, arg1_name, arg1_val) (void)0
#define TRACE_EVENT_ASYNC_END2(category_group, name, id, arg1_name, arg1_val, \
                               arg2_name, arg2_val) (void)0
#define TRACE_EVENT_COPY_ASYNC_END0(category_group, name, id) (void)0
#define TRACE_EVENT_COPY_ASYNC_END1(category_group, name, id, arg1_name, \
                                    arg1_val) (void)0
#define TRACE_EVENT_COPY_ASYNC_END2(category_group, name, id, arg1_name, \
                                    arg1_val, arg2_name, arg2_val) (void)0

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_H_
