// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/common/content_features.h"
#include "media/base/video_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/snapshot/snapshot_aura.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "components/remote_cocoa/browser/scoped_cg_window_id.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "base/strings/string_util_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#endif

using content::DesktopMediaID;

namespace {

// Update the list every second.
const int kDefaultNativeDesktopMediaListUpdatePeriod = 1000;

// Returns a hash of a DesktopFrame content to detect when image for a desktop
// media source has changed, if the frame is valid, or absl::null_opt if not.
absl::optional<size_t> GetFrameHash(webrtc::DesktopFrame* frame) {
  // These checks ensure invalid data isn't passed along, potentially leading to
  // crashes, e.g. when we calculate the hash which assumes a positive height
  // and stride.
  // TODO(crbug.com/1085230): figure out why the height is sometimes negative.
  if (!frame || !frame->data() || frame->stride() < 0 ||
      frame->size().height() < 0) {
    return absl::nullopt;
  }

  size_t data_size;
  if (!base::CheckMul<size_t>(frame->stride(), frame->size().height())
           .AssignIfValid(&data_size)) {
    return absl::nullopt;
  }

  return base::FastHash(base::make_span(frame->data(), data_size));
}

gfx::ImageSkia ScaleDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 gfx::Size size) {
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(frame->size().width(), frame->size().height()));

  SkBitmap result;
  result.allocN32Pixels(scaled_rect.width(), scaled_rect.height(), true);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());
  libyuv::ARGBScale(frame->data(), frame->stride(), frame->size().width(),
                    frame->size().height(), pixels_data, result.rowBytes(),
                    scaled_rect.width(), scaled_rect.height(),
                    libyuv::kFilterBilinear);

  // Set alpha channel values to 255 for all pixels.
  // TODO(sergeyu): Fix screen/window capturers to capture alpha channel and
  // remove this code. Currently screen/window capturers (at least some
  // implementations) only capture R, G and B channels and set Alpha to 0.
  // crbug.com/264424
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

#if BUILDFLAG(IS_WIN)
// These Collector functions are repeatedly invoked by `::EnumWindows` and they
// add HWNDs to the vector contained in `param`. Return TRUE to continue the
// enumeration or FALSE to end early.
//
// Collects all capturable HWNDs which are owned by the current process.
BOOL CALLBACK CapturableCurrentProcessHwndCollector(HWND hwnd, LPARAM param) {
  DWORD process_id;
  ::GetWindowThreadProcessId(hwnd, &process_id);
  if (process_id != ::GetCurrentProcessId())
    return TRUE;

  // Skip windows that aren't visible or are minimized.
  if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd))
    return TRUE;

  // Skip windows which are not presented in the taskbar, e.g. the "Restore
  // pages?" window.
  HWND owner = ::GetWindow(hwnd, GW_OWNER);
  LONG exstyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  if (owner && !(exstyle & WS_EX_APPWINDOW))
    return TRUE;

  auto* current_process_windows = reinterpret_cast<std::vector<HWND>*>(param);
  current_process_windows->push_back(hwnd);
  return TRUE;
}

// Collects all HWNDs, which are enumerated in z-order, to create a reference
// for sorting.
BOOL CALLBACK AllHwndCollector(HWND hwnd, LPARAM param) {
  auto* hwnds = reinterpret_cast<std::vector<HWND>*>(param);
  hwnds->push_back(hwnd);
  return TRUE;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kWindowCaptureMacV2,
             "WindowCaptureMacV2",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace

class NativeDesktopMediaList::Worker
    : public webrtc::DesktopCapturer::Callback,
      public webrtc::DelegatedSourceListController::Observer {
 public:
  Worker(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         base::WeakPtr<NativeDesktopMediaList> media_list,
         DesktopMediaList::Type type,
         std::unique_ptr<webrtc::DesktopCapturer> capturer,
         bool add_current_process_windows);

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  ~Worker() override;

  void Start();
  void Refresh(const DesktopMediaID::Id& view_dialog_id, bool update_thumnails);

  void RefreshThumbnails(std::vector<DesktopMediaID> native_ids,
                         const gfx::Size& thumbnail_size);
  void FocusList();
  void HideList();
  void ClearDelegatedSourceListSelection();

 private:
  typedef std::map<DesktopMediaID, size_t> ImageHashesMap;

  // Used to hold state associated with a call to RefreshThumbnails.
  struct RefreshThumbnailsState {
    std::vector<DesktopMediaID> source_ids;
    gfx::Size thumbnail_size;
    ImageHashesMap new_image_hashes;
    size_t next_source_index = 0;
  };

  // These must be members because |SourceDescription| is a protected type from
  // |DesktopMediaListBase|.
  static std::vector<SourceDescription> FormatSources(
      const webrtc::DesktopCapturer::SourceList& sources,
      const DesktopMediaID::Id& view_dialog_id,
      const DesktopMediaList::Type& list_type);

#if BUILDFLAG(IS_WIN)
  static std::vector<SourceDescription> GetCurrentProcessWindows();

  static std::vector<SourceDescription> MergeAndSortWindowSources(
      std::vector<SourceDescription> sources_a,
      std::vector<SourceDescription> sources_b);
#endif  // BUILDFLAG(IS_WIN)

  void RefreshNextThumbnail();

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // webrtc::DelegatedSourceListController::Observer interface.
  void OnSelection() override;
  void OnCancelled() override;
  void OnError() override;

  // Task runner used for capturing operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtr<NativeDesktopMediaList> media_list_;

  DesktopMediaList::Type type_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  const bool add_current_process_windows_;

  bool delegated_source_list_has_selection_ = false;
  bool focused_ = false;

  // Stores hashes of snapshots previously captured.
  ImageHashesMap image_hashes_;

  // Non-null when RefreshThumbnails hasn't yet completed.
  std::unique_ptr<RefreshThumbnailsState> refresh_thumbnails_state_;

  base::WeakPtrFactory<Worker> weak_factory_{this};
};

NativeDesktopMediaList::Worker::Worker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<NativeDesktopMediaList> media_list,
    DesktopMediaList::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    bool add_current_process_windows)
    : task_runner_(task_runner),
      media_list_(media_list),
      type_(type),
      capturer_(std::move(capturer)),
      add_current_process_windows_(add_current_process_windows) {
  DCHECK(capturer_);

  DCHECK(type_ == DesktopMediaList::Type::kWindow ||
         !add_current_process_windows_);
}

NativeDesktopMediaList::Worker::~Worker() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void NativeDesktopMediaList::Worker::Start() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  capturer_->Start(this);

  if (capturer_->GetDelegatedSourceListController())
    capturer_->GetDelegatedSourceListController()->Observe(this);
}

void NativeDesktopMediaList::Worker::Refresh(
    const DesktopMediaID::Id& view_dialog_id,
    bool update_thumnails) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  webrtc::DesktopCapturer::SourceList sources;
  if (!capturer_->GetSourceList(&sources)) {
    // Will pass empty results list to RefreshForVizFrameSinkWindows().
    sources.clear();
  }

  std::vector<SourceDescription> source_descriptions =
      FormatSources(sources, view_dialog_id, type_);

#if BUILDFLAG(IS_WIN)
  // If |add_current_process_windows_| is set to false, |capturer_| will have
  // found the windows owned by the current process for us. Otherwise, we must
  // do this.
  if (add_current_process_windows_) {
    DCHECK_EQ(type_, DesktopMediaList::Type::kWindow);
    // WebRTC returns the windows in order of highest z-order to lowest, but
    // these additional windows will be out of order if we just append them. So
    // we sort the list according to the z-order of the windows.
    source_descriptions = MergeAndSortWindowSources(
        std::move(source_descriptions), GetCurrentProcessWindows());
  }
#endif  // BUILDFLAG(IS_WIN)

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::RefreshForVizFrameSinkWindows,
                     media_list_, source_descriptions, update_thumnails));
}

void NativeDesktopMediaList::Worker::RefreshThumbnails(
    std::vector<DesktopMediaID> native_ids,
    const gfx::Size& thumbnail_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Ignore if refresh is already in progress.
  if (refresh_thumbnails_state_)
    return;

  // To refresh thumbnails, a snapshot of each window is captured and scaled
  // down to the specified size. Snapshotting can be asynchronous, and so
  // the process looks like the following steps:
  //
  // 1) RefreshNextThumbnail
  // 2) OnCaptureResult
  // 3) UpdateSourceThumbnail (if the snapshot changed)
  // [repeat 1, 2 and 3 until all thumbnails are refreshed]
  // 4) RefreshNextThumbnail
  // 5) UpdateNativeThumbnailsFinished
  //
  // |image_hashes_| is used to help avoid updating thumbnails that haven't
  // changed since the last refresh.

  refresh_thumbnails_state_ = std::make_unique<RefreshThumbnailsState>();
  refresh_thumbnails_state_->source_ids = std::move(native_ids);
  refresh_thumbnails_state_->thumbnail_size = thumbnail_size;
  RefreshNextThumbnail();
}

// static
std::vector<DesktopMediaListBase::SourceDescription>
NativeDesktopMediaList::Worker::FormatSources(
    const webrtc::DesktopCapturer::SourceList& sources,
    const DesktopMediaID::Id& view_dialog_id,
    const DesktopMediaList::Type& list_type) {
  std::vector<SourceDescription> source_descriptions;
  std::u16string title;
  for (size_t i = 0; i < sources.size(); ++i) {
    DesktopMediaID::Type source_type = DesktopMediaID::Type::TYPE_NONE;
    switch (list_type) {
      case DesktopMediaList::Type::kScreen:
        source_type = DesktopMediaID::Type::TYPE_SCREEN;
        // Just in case 'Screen' is inflected depending on the screen number,
        // use plural formatter.
        title = sources.size() > 1
                    ? l10n_util::GetPluralStringFUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
                          static_cast<int>(i + 1))
                    : l10n_util::GetStringUTF16(
                          IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        break;

      case DesktopMediaList::Type::kWindow:
        source_type = DesktopMediaID::Type::TYPE_WINDOW;
        // Skip the picker dialog window.
        if (sources[i].id == view_dialog_id)
          continue;
        title = base::UTF8ToUTF16(sources[i].title);
        break;

      default:
        NOTREACHED();
    }
    DesktopMediaID source_id(source_type, sources[i].id);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // We need to communicate this in_process_id to
    // |RefreshForVizFrameSinkWindows|, so we'll use the window_id. If
    // |in_process_id| is unset, then window_id will also remain unset and all
    // will be fine. See |RefreshForVizFrameSinkWindows| for a more in-depth
    // explanation.
    source_id.window_id = sources[i].in_process_id;
#endif
    source_descriptions.emplace_back(std::move(source_id), title);
  }

  return source_descriptions;
}

#if BUILDFLAG(IS_WIN)
// static
std::vector<DesktopMediaListBase::SourceDescription>
NativeDesktopMediaList::Worker::GetCurrentProcessWindows() {
  std::vector<HWND> current_process_windows;
  if (!::EnumWindows(CapturableCurrentProcessHwndCollector,
                     reinterpret_cast<LPARAM>(&current_process_windows))) {
    return std::vector<SourceDescription>();
  }

  std::vector<SourceDescription> current_process_sources;
  for (HWND hwnd : current_process_windows) {
    // Leave these sources untitled, we must get their title from the UI thread.
    current_process_sources.emplace_back(
        DesktopMediaID(
            DesktopMediaID::Type::TYPE_WINDOW,
            reinterpret_cast<webrtc::DesktopCapturer::SourceId>(hwnd)),
        u"");
  }

  return current_process_sources;
}

// static
std::vector<DesktopMediaListBase::SourceDescription>
NativeDesktopMediaList::Worker::MergeAndSortWindowSources(
    std::vector<SourceDescription> sources_a,
    std::vector<SourceDescription> sources_b) {
  // |EnumWindows| enumerates top level windows in z-order, we use this as a
  // reference for sorting.
  std::vector<HWND> z_ordered_windows;
  if (!::EnumWindows(AllHwndCollector,
                     reinterpret_cast<LPARAM>(&z_ordered_windows))) {
    // Since we can't get the z-order for the windows, we can't sort them. So,
    // let's just concatenate.
    sources_a.insert(sources_a.end(),
                     std::make_move_iterator(sources_b.begin()),
                     std::make_move_iterator(sources_b.end()));
    return sources_a;
  }

  std::vector<const std::vector<SourceDescription>*> source_containers = {
      &sources_a, &sources_b};
  std::vector<SourceDescription> sorted_sources;
  auto id_hwnd_projection = [](const SourceDescription& source) {
    return reinterpret_cast<const HWND>(source.id.id);
  };
  for (HWND window : z_ordered_windows) {
    for (const auto* source_container : source_containers) {
      auto source_it =
          base::ranges::find(*source_container, window, id_hwnd_projection);
      if (source_it != source_container->end()) {
        sorted_sources.push_back(*source_it);
        break;
      }
    }
  }

  return sorted_sources;
}
#endif  // BUILDFLAG(IS_WIN)

void NativeDesktopMediaList::Worker::RefreshNextThumbnail() {
  DCHECK(refresh_thumbnails_state_);

  for (size_t index = refresh_thumbnails_state_->next_source_index;
       index < refresh_thumbnails_state_->source_ids.size(); ++index) {
    refresh_thumbnails_state_->next_source_index = index + 1;
    DesktopMediaID source_id = refresh_thumbnails_state_->source_ids[index];
    if (capturer_->SelectSource(source_id.id)) {
      capturer_->CaptureFrame();  // Completes with OnCaptureResult.
      return;
    }
  }

  // Done capturing thumbnails.
  image_hashes_.swap(refresh_thumbnails_state_->new_image_hashes);
  refresh_thumbnails_state_.reset();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  auto index = refresh_thumbnails_state_->next_source_index - 1;
  DCHECK(index < refresh_thumbnails_state_->source_ids.size());
  DesktopMediaID id = refresh_thumbnails_state_->source_ids[index];

  // |frame| may be null if capture failed (e.g. because window has been
  // closed).
  auto frame_hash = GetFrameHash(frame.get());
  if (frame_hash) {
    refresh_thumbnails_state_->new_image_hashes[id] = *frame_hash;

    // Scale the image only if it has changed.
    auto it = image_hashes_.find(id);
    if (it == image_hashes_.end() || it->second != *frame_hash) {
      gfx::ImageSkia thumbnail = ScaleDesktopFrame(
          std::move(frame), refresh_thumbnails_state_->thumbnail_size);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&NativeDesktopMediaList::UpdateSourceThumbnail,
                         media_list_, id, thumbnail));
    }
  }

  // Protect against possible re-entrancy since OnCaptureResult can be invoked
  // from within the call to CaptureFrame.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Worker::RefreshNextThumbnail,
                                weak_factory_.GetWeakPtr()));
}

void NativeDesktopMediaList::Worker::ClearDelegatedSourceListSelection() {
  DCHECK(capturer_->GetDelegatedSourceListController());
  if (!delegated_source_list_has_selection_)
    return;

  delegated_source_list_has_selection_ = false;

  // If we're currently focused and the selection has been cleared; ensure that
  // the SourceList is visible.
  if (focused_)
    capturer_->GetDelegatedSourceListController()->EnsureVisible();
}

void NativeDesktopMediaList::Worker::FocusList() {
  focused_ = true;
  // If the capturer uses a delegated source list, then we need to ensure that
  // its source list is visible, unless a selection has previously been made.
  // If the capturer doesn't use a delegated source list, there's nothing for us
  // to do as we're continually querying the list state ourselves.
  if (capturer_->GetDelegatedSourceListController() &&
      !delegated_source_list_has_selection_)
    capturer_->GetDelegatedSourceListController()->EnsureVisible();
}

void NativeDesktopMediaList::Worker::HideList() {
  focused_ = false;
  // If the capturer uses a delegated source list, then we need to ensure that
  // its source list is hidden.
  // If the capturer doesn't use a delegated source list, there's nothing for us
  // to do as we want to continually querying the list state ourselves as we
  // have been doing.
  if (capturer_->GetDelegatedSourceListController())
    capturer_->GetDelegatedSourceListController()->EnsureHidden();
}

void NativeDesktopMediaList::Worker::OnSelection() {
  delegated_source_list_has_selection_ = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::OnDelegatedSourceListSelection,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnCancelled() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::OnDelegatedSourceListDismissed,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnError() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::OnDelegatedSourceListDismissed,
                     media_list_));
}

NativeDesktopMediaList::NativeDesktopMediaList(
    DesktopMediaList::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : NativeDesktopMediaList(type,
                             std::move(capturer),
                             /*add_current_process_windows=*/false) {}

NativeDesktopMediaList::NativeDesktopMediaList(
    DesktopMediaList::Type type,
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    bool add_current_process_windows)
    : DesktopMediaListBase(
          base::Milliseconds(kDefaultNativeDesktopMediaListUpdatePeriod)),
      thread_("DesktopMediaListCaptureThread"),
      add_current_process_windows_(add_current_process_windows),
      is_source_list_delegated_(capturer->GetDelegatedSourceListController() !=
                                nullptr) {
  type_ = type;

  DCHECK(type_ == DesktopMediaList::Type::kWindow ||
         !add_current_process_windows_);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
  // webrtc::DesktopCapturer implementations on Windows, MacOS and Fuchsia
  // expect to run on a thread with a UI message pump. Under Fuchsia the
  // capturer needs an async loop to support FIDL I/O.
  base::MessagePumpType thread_type = base::MessagePumpType::UI;
#else
  base::MessagePumpType thread_type = base::MessagePumpType::DEFAULT;
#endif
  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  worker_ = std::make_unique<Worker>(
      thread_.task_runner(), weak_factory_.GetWeakPtr(), type,
      std::move(capturer), add_current_process_windows_);

  if (!is_source_list_delegated_)
    StartCapturer();
}

NativeDesktopMediaList::~NativeDesktopMediaList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This thread should mostly be an idle observer. Stopping it should be fast.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;

  // Since we're on the UI thread (where all tasks to worker_ must have
  // posted from), this delete and then immediate stop (which triggers a thread
  // join) is safe because it ensures that no other tasks can be queued on the
  // thread after worker_'s deletion.
  thread_.task_runner()->DeleteSoon(FROM_HERE, worker_.release());
  thread_.Stop();
}

bool NativeDesktopMediaList::IsSourceListDelegated() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return is_source_list_delegated_;
}

void NativeDesktopMediaList::StartDelegatedCapturer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsSourceListDelegated());
  StartCapturer();
}

void NativeDesktopMediaList::StartCapturer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_capturer_started_);
  // base::Unretained is safe here because we own the lifetime of both the
  // worker and the thread and ensure that destroying the worker is the last
  // thing the thread does before stopping.
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::Start, base::Unretained(worker_.get())));
  is_capturer_started_ = true;
}

void NativeDesktopMediaList::ClearDelegatedSourceListSelection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because we own the lifetime of both the
  // worker and the thread and ensure that destroying the worker is the last
  // thing the thread does before stopping.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Worker::ClearDelegatedSourceListSelection,
                                base::Unretained(worker_.get())));
}

void NativeDesktopMediaList::FocusList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because we own the lifetime of both the
  // worker and the thread and ensure that destroying the worker is the last
  // thing the thread does before stopping.
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::FocusList, base::Unretained(worker_.get())));
}

void NativeDesktopMediaList::HideList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because we own the lifetime of both the
  // worker and the thread and ensure that destroying the worker is the last
  // thing the thread does before stopping.
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::HideList, base::Unretained(worker_.get())));
}

void NativeDesktopMediaList::Refresh(bool update_thumnails) {
  DCHECK(can_refresh());

#if defined(USE_AURA)
  DCHECK_EQ(pending_aura_capture_requests_, 0);
  DCHECK(!pending_native_thumbnail_capture_);
  new_aura_thumbnail_hashes_.clear();
#endif

  // base::Unretained is safe here because we own the lifetime of both the
  // worker and the thread and ensure that destroying the worker is the last
  // thing the thread does before stopping.
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::Refresh, base::Unretained(worker_.get()),
                     view_dialog_id_.id, update_thumnails));
}

void NativeDesktopMediaList::RefreshForVizFrameSinkWindows(
    std::vector<SourceDescription> sources,
    bool update_thumnails) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(can_refresh());

  auto source_it = sources.begin();
  while (source_it != sources.end()) {
    if (source_it->id.type != DesktopMediaID::TYPE_WINDOW) {
      ++source_it;
      continue;
    }

#if BUILDFLAG(IS_WIN)
    // The worker thread can't safely get titles for windows owned by the
    // current process, so we do it here, on the UI thread, where we can call
    // |GetWindowText| without risking a deadlock.
    const HWND hwnd = reinterpret_cast<HWND>(source_it->id.id);
    DWORD hwnd_process;
    ::GetWindowThreadProcessId(hwnd, &hwnd_process);
    if (hwnd_process == ::GetCurrentProcessId()) {
      int title_length = ::GetWindowTextLength(hwnd);

      // Remove untitled windows.
      if (title_length <= 0) {
        source_it = sources.erase(source_it);
        continue;
      }

      source_it->name.resize(title_length + 1);
      // The title may have changed since the call to |GetWindowTextLength|, so
      // we update |title_length| to be the number of characters written into
      // our string.
      title_length = ::GetWindowText(
          hwnd, base::as_writable_wcstr(source_it->name), title_length + 1);
      if (title_length <= 0) {
        source_it = sources.erase(source_it);
        continue;
      }

      // Resize the string (in the case the title has shortened), and remove the
      // trailing null character.
      source_it->name.resize(title_length);
    }
#endif  // BUILDFLAG(IS_WIN)

    // Assign |source_it->id.window_id| if |source_it->id.id| corresponds to a
    // viz::FrameSinkId.
    // TODO(https://crbug.com/1366579): This lookup is fairly fragile and has
    // now resulted in at least two patches to avoid it (though both are Wayland
    // based problems). On top of that, the series of ifdefs is a bit confusing.
    // We should try to simplify/abstract/cleanup this logic.
    // The root cause is that the Ozone Wayland Window Manager does *not* use a
    // platform handle/unique ID to back the AcceleratedWidget, but rather a
    // monotonically increasing int. Thus, capturers on that platform that
    // also (by default) use monotonically increasing ints as IDs (e.g.
    // delegated source lists, the lacros capturer) can have source IDs that
    // collide with known aura IDs. This causes us to mistakenly try to capture
    // the non-aura windows as an aura window. The preview ultimately matches
    // what is captured, but this is likely unexpected for the user and can
    // result in multiple instances of a window appearing in the source list and
    // also means that the collided non-aura window cannot be captured.
#if defined(USE_AURA)
    if (!is_source_list_delegated_) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // The lacros capturer is not delegated and can circumvent the collision
      // described above because it receives additional information about each
      // window from Ash-chrome; however, it is limited in how it can convey
      // that information. |FormatSources|, above, will put the internal ID into
      // the window_id slot; but this will not yet be a registered native
      // window, as the capturer does not run on the UI thread. Thus, we still
      // need to find and register this window here and then overwrite the
      // window_id. If the window_id has not been set, we'll just fail to find a
      // corresponding window and the state will remain unset.
      DesktopMediaID::Id search_id = source_it->id.window_id;
#else
      DesktopMediaID::Id search_id = source_it->id.id;
#endif
      aura::WindowTreeHost* const host =
          aura::WindowTreeHost::GetForAcceleratedWidget(
              *reinterpret_cast<gfx::AcceleratedWidget*>(&search_id));
      aura::Window* const aura_window = host ? host->window() : nullptr;
      if (aura_window) {
        DesktopMediaID aura_id = DesktopMediaID::RegisterNativeWindow(
            DesktopMediaID::TYPE_WINDOW, aura_window);
        source_it->id.window_id = aura_id.window_id;
      } else if (search_id != DesktopMediaID::kNullId) {
        // This is expected for non-LaCrOS platforms, where we are searching all
        // IDs (which include windows/screens that we don't own). However, on
        // LaCrOS, if we set search_id, then that means we think we should know
        // about the window. There are potential race conditions where this
        // could happen, so don't throw an error, but do log it in case any
        // issues pop up in the future so we can debug it.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        LOG(ERROR) << __func__ << ": Could not find window but had window id";
        source_it->id.window_id = DesktopMediaID::kNullId;
#endif
      }
    }
#elif BUILDFLAG(IS_MAC)
    if (base::FeatureList::IsEnabled(kWindowCaptureMacV2)) {
      if (remote_cocoa::ScopedCGWindowID::Get(source_it->id.id))
        source_it->id.window_id = source_it->id.id;
    }
#endif

    ++source_it;
  }

  UpdateSourcesList(sources);

  if (!update_thumnails) {
    OnRefreshComplete();
    return;
  }

  if (thumbnail_size_.IsEmpty()) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // OnAuraThumbnailCaptured() and UpdateNativeThumbnailsFinished() are
  // guaranteed to be executed after RefreshForVizFrameSinkWindows() and
  // CaptureAuraWindowThumbnail() in the browser UI thread.
  // Therefore pending_aura_capture_requests_ will be set the number of aura
  // windows to be captured and pending_native_thumbnail_capture_ will be set
  // true if native thumbnail capture is needed before OnAuraThumbnailCaptured()
  // or UpdateNativeThumbnailsFinished() are called.
  std::vector<DesktopMediaID> native_ids;
  for (const auto& source : sources) {
#if defined(USE_AURA)
    if (source.id.window_id > DesktopMediaID::kNullId) {
      CaptureAuraWindowThumbnail(source.id);
      continue;
    }
#endif  // defined(USE_AURA)
    native_ids.push_back(source.id);
  }

  if (!native_ids.empty()) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    // base::Unretained is safe here because we own the lifetime of both the
    // worker and the thread and ensure that destroying the worker is the last
    // thing the thread does before stopping.
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Worker::RefreshThumbnails,
                                  base::Unretained(worker_.get()),
                                  std::move(native_ids), thumbnail_size_));
  }
}

void NativeDesktopMediaList::UpdateNativeThumbnailsFinished() {
#if defined(USE_AURA)
  DCHECK(pending_native_thumbnail_capture_);
  pending_native_thumbnail_capture_ = false;
  // If native thumbnail captures finished after aura thumbnail captures,
  // execute |done_callback| to let the caller know the update process is
  // finished.  If necessary, this will schedule the next refresh.
  if (pending_aura_capture_requests_ == 0)
    OnRefreshComplete();
#else
  OnRefreshComplete();
#endif  // defined(USE_AURA)
}

#if defined(USE_AURA)

void NativeDesktopMediaList::CaptureAuraWindowThumbnail(
    const DesktopMediaID& id) {
  DCHECK(can_refresh());

  gfx::NativeWindow window = DesktopMediaID::GetNativeWindowById(id);
  if (!window)
    return;

  gfx::Rect window_rect(window->bounds().width(), window->bounds().height());
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(thumbnail_size_), window_rect.size());

  pending_aura_capture_requests_++;
  ui::GrabWindowSnapshotAndScaleAsyncAura(
      window, window_rect, scaled_rect.size(),
      base::BindOnce(&NativeDesktopMediaList::OnAuraThumbnailCaptured,
                     weak_factory_.GetWeakPtr(), id));
}

void NativeDesktopMediaList::OnAuraThumbnailCaptured(const DesktopMediaID& id,
                                                     gfx::Image image) {
  DCHECK(can_refresh());

  if (!image.IsEmpty()) {
    // Only new or changed thumbnail need update.
    new_aura_thumbnail_hashes_[id] = GetImageHash(image);
    if (!previous_aura_thumbnail_hashes_.count(id) ||
        previous_aura_thumbnail_hashes_[id] != new_aura_thumbnail_hashes_[id]) {
      UpdateSourceThumbnail(id, image.AsImageSkia());
    }
  }

  // After all aura windows are processed, schedule next refresh;
  pending_aura_capture_requests_--;
  DCHECK_GE(pending_aura_capture_requests_, 0);
  if (pending_aura_capture_requests_ == 0) {
    previous_aura_thumbnail_hashes_ = std::move(new_aura_thumbnail_hashes_);
    // Schedule next refresh if aura thumbnail captures finished after native
    // thumbnail captures.
    if (!pending_native_thumbnail_capture_)
      OnRefreshComplete();
  }
}

#endif  // defined(USE_AURA)

scoped_refptr<base::SingleThreadTaskRunner>
NativeDesktopMediaList::GetCapturerTaskRunnerForTesting() const {
  return thread_.task_runner();
}
