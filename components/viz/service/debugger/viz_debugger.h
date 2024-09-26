// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_
#define COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_

#include <atomic>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/debug/debugging_buildflags.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/service/debugger/rwlock.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

// The visual debugger can be completely disabled/enabled at compile time via
// the |USE_VIZ_DEBUGGER| build flag which corresponds to boolean gn arg
// 'use_viz_debugger'. Consult README.md for more information.

#if BUILDFLAG(USE_VIZ_DEBUGGER)

#define VIZ_DEBUGGER_IS_ON() true

namespace viz {

class VIZ_SERVICE_EXPORT VizDebugger {
 public:
  // These functions are called on a gpu thread that is not the
  // 'VizCompositorThread' and therefore have mulithreaded considerations.
  void FilterDebugStream(base::Value json);
  void StartDebugStream(
      mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output);
  void StopDebugStream();

  struct VIZ_SERVICE_EXPORT StaticSource {
    StaticSource(const char* anno_name,
                 const char* file_name,
                 int file_line,
                 const char* func_name);
    inline bool IsActive() const { return active; }
    inline bool IsEnabled() const { return enabled; }
    const char* anno = nullptr;
    const char* file = nullptr;
    const char* func = nullptr;
    const int line = 0;

    int reg_index = 0;
    bool active = false;
    bool enabled = false;
  };

  struct DrawOption {
    // TODO(petermcneeley): Consider moving this custom rgba color data over to
    // |SkColor| representation.
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    // Alpha is applied to rect fill only.
    uint8_t color_a;
  };

  ALWAYS_INLINE static bool IsEnabled() {
    return enabled_.load(std::memory_order_acquire);
  }

  static VizDebugger* GetInstance();

  ~VizDebugger();

  struct VIZ_SERVICE_EXPORT BufferInfo {
    BufferInfo();
    ~BufferInfo();
    BufferInfo(const BufferInfo& a);
    SkBitmap bitmap;
  };

  struct Buffer {
    int id;
    BufferInfo buffer_info;
  };

  void SubmitBuffer(int buff_id, BufferInfo&& buffer);

  void CompleteFrame(const uint64_t counter,
                     const gfx::Size& window_pix,
                     base::TimeTicks time_ticks);
  void DrawText(const gfx::Point& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void DrawText(const gfx::Vector2dF& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void DrawText(const gfx::PointF& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void Draw(const gfx::SizeF& obj_size,
            const gfx::Vector2dF& pos,
            const StaticSource* dcs,
            DrawOption option,
            int* id,
            const gfx::RectF& uv);
  void Draw(const gfx::Size& obj_size,
            const gfx::Vector2dF& pos,
            const StaticSource* dcs,
            DrawOption option,
            int* id,
            const gfx::RectF& uv);

  void AddLogMessage(std::string log,
                     const StaticSource* dcs,
                     DrawOption option);

  VizDebugger(const VizDebugger&) = delete;
  VizDebugger& operator=(const VizDebugger&) = delete;

 private:
  friend class VizDebuggerInternal;
  static std::atomic<bool> enabled_;
  VizDebugger();
  base::Value FrameAsJson(const uint64_t counter,
                          const gfx::Size& window_pix,
                          base::TimeTicks time_ticks);

  void AddFrame();
  void UpdateFilters();
  void RegisterSource(StaticSource* source);
  void DrawInternal(const gfx::Size& obj_size,
                    const gfx::Vector2dF& pos,
                    const StaticSource* dcs,
                    DrawOption option,
                    int* id,
                    const gfx::RectF& uv);
  void ApplyFilters(VizDebugger::StaticSource* source);
  mojo::Remote<mojom::VizDebugOutput> debug_output_;

  // This |task_runner_| is required to send json through mojo.
  scoped_refptr<base::SequencedTaskRunner> gpu_thread_task_runner_;

  struct CallSubmitCommon {
    CallSubmitCommon() = default;
    CallSubmitCommon(int index, int source, int thread, DrawOption draw_option)
        : draw_index(index),
          source_index(source),
          thread_id(thread),
          option(draw_option) {}
    base::Value::Dict GetDictionaryValue() const;
    int draw_index;
    int source_index;
    int thread_id;
    VizDebugger::DrawOption option;
  };

  struct DrawCall : public CallSubmitCommon {
    DrawCall() = default;
    DrawCall(int index,
             int source,
             int thread,
             DrawOption draw_option,
             gfx::Size size,
             gfx::Vector2dF position,
             int buffer_id,
             gfx::RectF uv_rect)
        : CallSubmitCommon(index, source, thread, draw_option),
          obj_size(size),
          pos(position),
          buff_id(buffer_id),
          uv(uv_rect) {}
    gfx::Size obj_size;
    gfx::Vector2dF pos;
    int buff_id;
    gfx::RectF uv;
  };

  struct DrawTextCall : public CallSubmitCommon {
    DrawTextCall() = default;
    DrawTextCall(int index,
                 int source,
                 int thread,
                 DrawOption draw_option,
                 gfx::Vector2dF position,
                 std::string str)
        : CallSubmitCommon(index, source, thread, draw_option),
          pos(position),
          text(str) {}
    gfx::Vector2dF pos;
    std::string text;
  };

  struct LogCall : public CallSubmitCommon {
    LogCall() = default;
    LogCall(int index,
            int source,
            int thread,
            DrawOption draw_option,
            std::string str)
        : CallSubmitCommon(index, source, thread, draw_option),
          value(std::move(str)) {}
    std::string value;
  };

  struct VIZ_SERVICE_EXPORT FilterBlock {
    FilterBlock(const std::string file_str,
                const std::string func_str,
                const std::string anno_str,
                bool is_active,
                bool is_enabled);
    ~FilterBlock();
    FilterBlock(const FilterBlock& other);
    std::string file;
    std::string func;
    std::string anno;
    bool active = false;
    bool enabled = false;
  };

  // Synchronize access to buffers and variables mutated by multiple threads.
  rwlock::RWLock read_write_lock_;

  // New filters to promoted to cached filters on next frame.
  std::vector<FilterBlock> new_filters_;
  bool apply_new_filters_next_frame_ = false;

  // Json is saved out every frame on the call to 'CompleteFrame' but may not be
  // uploaded immediately due to task runner sequencing.
  base::Value json_frame_output_;
  size_t last_sent_source_count_ = 0;

  // Cached filters to apply filtering to new sources not just on filter update.
  std::vector<FilterBlock> cached_filters_;
  int buffer_id = 0;

  // Common counter for all submissions. This variable can be accessed by
  // multiple threads atomically.
  std::atomic<int> submission_count_ = 0;

  // Default starting size for each buffer/vector.
  static constexpr int kDefaultBufferSize = 64;

  // Buffers/vectors for each type of debug calls. These vectors can be accessed
  // and mutated by multiple threads simultaneously or individually.
  std::vector<DrawCall> draw_rect_calls_{kDefaultBufferSize};
  std::vector<DrawTextCall> draw_text_calls_{kDefaultBufferSize};
  std::vector<LogCall> logs_{kDefaultBufferSize};
  std::vector<StaticSource*> sources_;
  std::vector<Buffer> buffers_;

  // Individual tail indices tracker variables for next insertion index in
  // each buffer. These variables can be accessed by multiple threads
  // atomically.
  std::atomic<int> draw_rect_calls_tail_idx_ = 0;
  std::atomic<int> draw_text_calls_tail_idx_ = 0;
  std::atomic<int> logs_tail_idx_ = 0;
};

}  // namespace viz

#define DBG_OPT_RED viz::VizDebugger::DrawOption({255, 0, 0, 0})
#define DBG_OPT_GREEN viz::VizDebugger::DrawOption({0, 255, 0, 0})
#define DBG_OPT_BLUE viz::VizDebugger::DrawOption({0, 0, 255, 0})
#define DBG_OPT_BLACK viz::VizDebugger::DrawOption({0, 0, 0, 0})
#define DEFAULT_UV gfx::RectF(0, 0, 1, 1)

#define DBG_DRAW_RECTANGLE_OPT_BUFF_UV(anno, option, pos, size, id, uv)    \
  do {                                                                     \
    if (viz::VizDebugger::IsEnabled()) {                                   \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__,  \
                                                __func__);                 \
      if (dcs.IsActive()) {                                                \
        viz::VizDebugger::GetInstance()->Draw(size, pos, &dcs, option, id, \
                                              uv);                         \
      }                                                                    \
    }                                                                      \
  } while (0)

#define DBG_DRAW_RECTANGLE_OPT_BUFF(anno, option, pos, size, id) \
  DBG_DRAW_RECTANGLE_OPT_BUFF_UV(anno, option, pos, size, id, DEFAULT_UV)

#define DBG_COMPLETE_BUFFERS(buff_id, buffer)                           \
  do {                                                                  \
    if (viz::VizDebugger::IsEnabled()) {                                \
      viz::VizDebugger::GetInstance()->SubmitBuffer(buff_id,            \
                                                    std::move(buffer)); \
    }                                                                   \
  } while (0)

#define DBG_DRAW_RECTANGLE_OPT(anno, option, pos, size) \
  DBG_DRAW_RECTANGLE_OPT_BUFF(anno, option, pos, size, nullptr)

#define DBG_DRAW_RECTANGLE(anno, pos, size) \
  DBG_DRAW_RECTANGLE_OPT_BUFF(anno, DBG_OPT_BLACK, pos, size, nullptr)

#define DBG_DRAW_TEXT_OPT(anno, option, pos, text)                          \
  do {                                                                      \
    if (viz::VizDebugger::IsEnabled()) {                                    \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__,   \
                                                __func__);                  \
      if (dcs.IsActive()) {                                                 \
        viz::VizDebugger::GetInstance()->DrawText(pos, text, &dcs, option); \
      }                                                                     \
    }                                                                       \
  } while (0)

#define DBG_DRAW_TEXT(anno, pos, text) \
  DBG_DRAW_TEXT_OPT(anno, DBG_OPT_BLACK, pos, text)

#define DBG_LOG_OPT(anno, option, format, ...)                            \
  do {                                                                    \
    if (viz::VizDebugger::IsEnabled()) {                                  \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__, \
                                                __func__);                \
      if (dcs.IsActive()) {                                               \
        viz::VizDebugger::GetInstance()->AddLogMessage(                   \
            base::StringPrintf(format, __VA_ARGS__), &dcs, option);       \
      }                                                                   \
    }                                                                     \
  } while (0)

#define DBG_LOG(anno, format, ...) \
  DBG_LOG_OPT(anno, DBG_OPT_BLACK, format, __VA_ARGS__)

#define DBG_DRAW_RECT_OPT_BUFF(anno, option, rect, id)                    \
  DBG_DRAW_RECTANGLE_OPT_BUFF(                                            \
      anno, option, gfx::Vector2dF(rect.origin().x(), rect.origin().y()), \
      rect.size(), id)

#define DBG_DRAW_RECT_OPT_BUFF_UV(anno, option, rect, id, uv)             \
  DBG_DRAW_RECTANGLE_OPT_BUFF_UV(                                         \
      anno, option, gfx::Vector2dF(rect.origin().x(), rect.origin().y()), \
      rect.size(), id, uv)

#define DBG_DRAW_RECT_OPT(anno, option, rect)                                  \
  DBG_DRAW_RECTANGLE_OPT(anno, option,                                         \
                         gfx::Vector2dF(rect.origin().x(), rect.origin().y()), \
                         rect.size())

#define DBG_DRAW_RECT_BUFF(anno, rect, id) \
  DBG_DRAW_RECT_OPT_BUFF(anno, DBG_OPT_BLACK, rect, id)

#define DBG_DRAW_RECT_BUFF_UV(anno, rect, id, uv) \
  DBG_DRAW_RECT_OPT_BUFF_UV(anno, DBG_OPT_BLACK, rect, id, uv)

#define DBG_DRAW_RECT(anno, rect) DBG_DRAW_RECT_OPT(anno, DBG_OPT_BLACK, rect)

#define DBG_FLAG_FBOOL(anno, fun_name)                                    \
  namespace {                                                             \
  bool fun_name() {                                                       \
    if (viz::VizDebugger::IsEnabled()) {                                  \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__, \
                                                __func__);                \
      if (dcs.IsEnabled()) {                                              \
        return true;                                                      \
      }                                                                   \
    }                                                                     \
    return false;                                                         \
  }                                                                       \
  }  // namespace

#else  //  !BUILDFLAG(USE_VIZ_DEBUGGER)

#define VIZ_DEBUGGER_IS_ON() false

// Viz Debugger is not enabled. The |VizDebugger| class is minimally defined to
// reduce the need for if/def checks in external code. All debugging macros
// compiled to empty statements but do eat some parameters to prevent used
// variable warnings.

namespace viz {
class VIZ_SERVICE_EXPORT VizDebugger {
 public:
  VizDebugger() = default;
  static inline VizDebugger* GetInstance() {
    static VizDebugger g_debugger;
    return &g_debugger;
  }

  // These structures are part of public API and must be included.
  struct VIZ_SERVICE_EXPORT BufferInfo {
    BufferInfo();
    ~BufferInfo();
    BufferInfo(const BufferInfo& a);
    SkBitmap bitmap;
  };

  struct DrawOption {
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t color_a;
  };

  inline void CompleteFrame(uint64_t counter,
                            const gfx::Size& window_pix,
                            base::TimeTicks time_ticks) {}

  static inline bool IsEnabled() { return false; }
  VizDebugger(const VizDebugger&) = delete;
  VizDebugger& operator=(const VizDebugger&) = delete;
};

}  // namespace viz

#define DBG_OPT_RED 0

#define DBG_OPT_GREEN 0

#define DBG_OPT_BLUE 0

#define DBG_OPT_BLACK 0

#define DEFAULT_UV 0

#define DBG_DRAW_RECTANGLE_OPT_BUFF_UV(anno, option, pos, size, id, uv) \
  std::ignore = anno;                                                   \
  std::ignore = option;                                                 \
  std::ignore = pos;                                                    \
  std::ignore = size;                                                   \
  std::ignore = id;                                                     \
  std::ignore = uv;

#define DBG_DRAW_RECTANGLE_OPT_BUFF(anno, option, pos, size, id) \
  std::ignore = anno;                                            \
  std::ignore = option;                                          \
  std::ignore = pos;                                             \
  std::ignore = size;                                            \
  std::ignore = id;

#define DBG_COMPLETE_BUFFERS(buff_id, buffer) \
  std::ignore = buff_id;                      \
  std::ignore = buffer;

#define DBG_DRAW_RECTANGLE_OPT(anno, option, pos, size) \
  DBG_DRAW_RECTANGLE_OPT_BUFF(anno, option, pos, size, nullptr)

#define DBG_DRAW_TEXT_OPT(anno, option, pos, text) \
  std::ignore = anno;                              \
  std::ignore = option;                            \
  std::ignore = pos;                               \
  std::ignore = text;

#define DBG_DRAW_TEXT(anno, pos, text) \
  DBG_DRAW_TEXT_OPT(anno, DBG_OPT_BLACK, pos, text)

#define DBG_LOG_OPT(anno, option, format, ...) \
  std::ignore = anno;                          \
  std::ignore = option;                        \
  std::ignore = format;

#define DBG_LOG(anno, format, ...) DBG_LOG_OPT(anno, DBG_OPT_BLACK, format, ...)

#define DBG_DRAW_RECT_OPT_BUFF_UV(anno, option, rect, id, uv) \
  std::ignore = anno;                                         \
  std::ignore = option;                                       \
  std::ignore = rect;                                         \
  std::ignore = id;                                           \
  std::ignore = uv;

#define DBG_DRAW_RECT_OPT_BUFF(anno, option, rect, id) \
  std::ignore = anno;                                  \
  std::ignore = option;                                \
  std::ignore = rect;                                  \
  std::ignore = id;

#define DBG_DRAW_RECT_OPT(anno, option, rect) \
  DBG_DRAW_RECT_OPT_BUFF(anno, option, rect, nullptr)

#define DBG_DRAW_RECT_BUFF_UV(anno, rect, id, uv) \
  DBG_DRAW_RECT_OPT_BUFF_UV(anno, DBG_OPT_BLACK, rect, id, uv)

#define DBG_DRAW_RECT_BUFF(anno, rect, id) \
  DBG_DRAW_RECT_OPT_BUFF(anno, DBG_OPT_BLACK, rect, id)

#define DBG_DRAW_RECT(anno, rect) \
  DBG_DRAW_RECT_OPT_BUFF(anno, DBG_OPT_BLACK, rect, nullptr)

#define DBG_FLAG_FBOOL(anno, fun_name)        \
  namespace {                                 \
  constexpr bool fun_name() { return false; } \
  }

#endif  // BUILDFLAG(USE_VIZ_DEBUGGER)

#endif  // COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_
