// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/envelope.h"

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include <mach/mach.h>
#include <pthread.h>

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/process/port_provider_mac.h"
#include "base/process/process.h"
#include "base/task/thread_type.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#endif

namespace mojo::core::ipcz_driver {

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)

namespace {

using ChromeTrackEvent = perfetto::protos::pbzero::ChromeTrackEvent;
using MacVoucherRelease = perfetto::protos::pbzero::MacVoucherRelease;

class SchedulingTraceInfo {
 public:
  SchedulingTraceInfo() {
    const base::Process current_process = base::Process::Current();
    base::SelfPortProvider port_provider;
    switch (current_process.GetPriority(&port_provider)) {
      case base::Process::Priority::kBestEffort:
        process_priority_ = MacVoucherRelease::PROCESS_PRIORITY_BEST_EFFORT;
        break;
      case base::Process::Priority::kUserVisible:
        process_priority_ = MacVoucherRelease::PROCESS_PRIORITY_USER_VISIBLE;
        break;
      case base::Process::Priority::kUserBlocking:
        process_priority_ = MacVoucherRelease::PROCESS_PRIORITY_USER_BLOCKING;
        break;
    }
    process_os_priority_ = current_process.GetOSPriority();

    switch (base::PlatformThread::GetCurrentThreadType()) {
      case base::ThreadType::kBackground:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_BACKGROUND;
        break;
      case base::ThreadType::kUtility:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_UTILITY;
        break;
      case base::ThreadType::kDefault:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_DEFAULT;
        break;
      case base::ThreadType::kPresentation:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_PRESENTATION;
        break;
      case base::ThreadType::kAudioProcessing:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_AUDIO_PROCESSING;
        break;
      case base::ThreadType::kRealtimeAudio:
        thread_type_ = MacVoucherRelease::THREAD_TYPE_REALTIME_AUDIO;
        break;
    }
    thread_has_leases_ = base::PlatformThread::CurrentThreadHasLeases();

    qos_class_t qos_class = QOS_CLASS_UNSPECIFIED;
    int relative_priority = 0;
    if (pthread_get_qos_class_np(pthread_self(), &qos_class,
                                 &relative_priority) == 0) {
      switch (qos_class) {
        case QOS_CLASS_UNSPECIFIED:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_UNSPECIFIED;
          break;
        case QOS_CLASS_BACKGROUND:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_BACKGROUND;
          break;
        case QOS_CLASS_UTILITY:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_UTILITY;
          break;
        case QOS_CLASS_DEFAULT:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_DEFAULT;
          break;
        case QOS_CLASS_USER_INITIATED:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_USER_INITIATED;
          break;
        case QOS_CLASS_USER_INTERACTIVE:
          thread_qos_class_ = MacVoucherRelease::QOS_CLASS_USER_INTERACTIVE;
          break;
        default:
          NOTREACHED();
      }
      thread_relative_priority_ = relative_priority;
    }
  }

  void AddToTrace(MacVoucherRelease& trace_event) {
    trace_event.set_process_priority(process_priority_);
    trace_event.set_process_os_priority(process_os_priority_);
    trace_event.set_thread_type(thread_type_);
    trace_event.set_thread_has_leases(thread_has_leases_);
    if (thread_qos_class_) {
      trace_event.set_thread_qos_class(*thread_qos_class_);
    }
    if (thread_relative_priority_) {
      trace_event.set_thread_relative_priority(*thread_relative_priority_);
    }
  }

 private:
  MacVoucherRelease::ProcessPriority process_priority_ =
      MacVoucherRelease::PROCESS_PRIORITY_BEST_EFFORT;
  int32_t process_os_priority_ = 0;
  MacVoucherRelease::ThreadType thread_type_ =
      MacVoucherRelease::THREAD_TYPE_DEFAULT;
  bool thread_has_leases_ = false;
  std::optional<MacVoucherRelease::QoSClass> thread_qos_class_;
  std::optional<int32_t> thread_relative_priority_;
};

void ReleaseVoucher(base::apple::ScopedMachSendRight voucher) {
  if (!voucher.is_valid()) {
    return;
  }

  // mach_port_get_refs() always succeeds with a valid port.
  mach_port_urefs_t refs = 0;
  CHECK_EQ(KERN_SUCCESS, mach_port_get_refs(mach_task_self(), voucher.get(),
                                            MACH_PORT_RIGHT_SEND, &refs));

  // Only trace the full scheduling info when the last ref is dropped, which can
  // change QoS. Read the values before the trace event so the lookup time isn't
  // included in the event duration.
  auto scheduling_trace_info =
      refs == 1 ? std::make_optional<SchedulingTraceInfo>() : std::nullopt;

  {
    TRACE_EVENT("ipc", "Release QoS Voucher", [&](perfetto::EventContext ctx) {
      auto* voucher_release =
          ctx.event<ChromeTrackEvent>()->set_mac_voucher_release();
      voucher_release->set_ref_count(refs);
      if (scheduling_trace_info) {
        scheduling_trace_info->AddToTrace(*voucher_release);
      }
    });
    voucher.reset();
  }

  // Trace the full scheduling info again after dropping the last ref, to see
  // whether QoS changed. If the thread was descheduled, this info may be
  // stale by the time it runs again, but there's no way to avoid that.
  if (refs == 1) {
    TRACE_EVENT_INSTANT(
        "ipc", "After QoS Voucher Release", [&](perfetto::EventContext ctx) {
          auto* voucher_release =
              ctx.event<ChromeTrackEvent>()->set_mac_voucher_release();
          SchedulingTraceInfo().AddToTrace(*voucher_release);
        });
  }
}

}  // namespace

#endif  // BUILDFLAG(MOJO_USE_APPLE_CHANNEL)

Envelope::Envelope() = default;

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
Envelope::Envelope(base::apple::ScopedMachSendRight voucher)
    : voucher_(std::move(voucher)) {}
#endif

Envelope::~Envelope() = default;

void Envelope::Close() {
#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  ReleaseVoucher(std::move(voucher_));
#endif
}

}  // namespace mojo::core::ipcz_driver
