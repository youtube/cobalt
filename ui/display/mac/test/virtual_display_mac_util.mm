// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/test/virtual_display_mac_util.h"

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include <map>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

// These interfaces were generated from CoreGraphics binaries.
@interface CGVirtualDisplay : NSObject

@property(readonly, nonatomic) unsigned int vendorID;
@property(readonly, nonatomic) unsigned int productID;
@property(readonly, nonatomic) unsigned int serialNum;
@property(readonly, nonatomic) NSString* name;
@property(readonly, nonatomic) struct CGSize sizeInMillimeters;
@property(readonly, nonatomic) unsigned int maxPixelsWide;
@property(readonly, nonatomic) unsigned int maxPixelsHigh;
@property(readonly, nonatomic) struct CGPoint redPrimary;
@property(readonly, nonatomic) struct CGPoint greenPrimary;
@property(readonly, nonatomic) struct CGPoint bluePrimary;
@property(readonly, nonatomic) struct CGPoint whitePoint;
@property(readonly, nonatomic) id queue;
@property(readonly, nonatomic) id terminationHandler;
@property(readonly, nonatomic) unsigned int displayID;
@property(readonly, nonatomic) unsigned int hiDPI;
@property(readonly, nonatomic) NSArray* modes;
@property(readonly, nonatomic)
    unsigned int serialNumber API_AVAILABLE(macos(11.0));
@property(readonly, nonatomic) unsigned int rotation API_AVAILABLE(macos(11.0));
- (BOOL)applySettings:(id)arg1;
- (void)dealloc;
- (id)initWithDescriptor:(id)arg1;

@end

// These interfaces were generated from CoreGraphics binaries.
@interface CGVirtualDisplayDescriptor : NSObject

@property(nonatomic) unsigned int vendorID;
@property(nonatomic) unsigned int productID;
@property(nonatomic) unsigned int serialNum;
@property(strong, nonatomic) NSString* name;
@property(nonatomic) struct CGSize sizeInMillimeters;
@property(nonatomic) unsigned int maxPixelsWide;
@property(nonatomic) unsigned int maxPixelsHigh;
@property(nonatomic) struct CGPoint redPrimary;
@property(nonatomic) struct CGPoint greenPrimary;
@property(nonatomic) struct CGPoint bluePrimary;
@property(nonatomic) struct CGPoint whitePoint;
@property(strong, nonatomic) id queue;
@property(copy, nonatomic) id terminationHandler;
@property(nonatomic) unsigned int serialNumber API_AVAILABLE(macos(11.0));
- (void)dealloc;
- (id)init;
- (id)dispatchQueue;
- (void)setDispatchQueue:(id)arg1;

@end

// These interfaces were generated from CoreGraphics binaries.
@interface CGVirtualDisplayMode : NSObject

@property(readonly, nonatomic) unsigned int width;
@property(readonly, nonatomic) unsigned int height;
@property(readonly, nonatomic) double refreshRate;
@property(copy, nonatomic) id transferFunction API_AVAILABLE(macos(13.3));
- (id)initWithWidth:(unsigned int)arg1
             height:(unsigned int)arg2
        refreshRate:(double)arg3;
- (id)initWithWidth:(unsigned int)arg1
              height:(unsigned int)arg2
         refreshRate:(double)arg3
    transferFunction:(id)arg4 API_AVAILABLE(macos(13.3));

@end

// These interfaces were generated from CoreGraphics binaries.
@interface CGVirtualDisplaySettings : NSObject

@property(strong, nonatomic) NSArray* modes;
@property(nonatomic) unsigned int hiDPI;
@property(nonatomic) unsigned int rotation API_AVAILABLE(macos(11.0));
- (void)dealloc;
- (id)init;

@end

namespace {

static bool g_need_display_removal_workaround = true;

// A global singleton that tracks the current set of mocked displays.
std::map<int64_t, CGVirtualDisplay * __strong> g_display_map;

// A helper function for creating virtual display and return CGVirtualDisplay
// object.
CGVirtualDisplay* CreateVirtualDisplay(int width,
                                       int height,
                                       int ppi,
                                       BOOL hiDPI,
                                       NSString* name,
                                       int serial_number) {
  CGVirtualDisplayDescriptor* descriptor =
      [[CGVirtualDisplayDescriptor alloc] init];
  descriptor.queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
  descriptor.name = name;
  descriptor.whitePoint = CGPointMake(0.3125, 0.3291);
  descriptor.bluePrimary = CGPointMake(0.1494, 0.0557);
  descriptor.greenPrimary = CGPointMake(0.2559, 0.6983);
  descriptor.redPrimary = CGPointMake(0.6797, 0.3203);
  descriptor.maxPixelsHigh = height;
  descriptor.maxPixelsWide = width;
  descriptor.sizeInMillimeters =
      CGSizeMake(25.4 * width / ppi, 25.4 * height / ppi);
  // macOS 14 expects different virtual displays to have different serial
  // numbers.
  descriptor.serialNum = serial_number;
  descriptor.productID = 0;
  // macOS 14 expects a non-zero vendorID. Choose an arbitrary value.
  int kVendorID = 505;
  descriptor.vendorID = kVendorID;
  descriptor.terminationHandler = nil;
  if (@available(macos 11.0, *)) {
    descriptor.serialNumber = serial_number;
  }

  CGVirtualDisplay* display =
      [[CGVirtualDisplay alloc] initWithDescriptor:descriptor];
  if (!display) {
    LOG(ERROR) << __func__ << " - CGVirtualDisplay initWithDescriptor failed";
    return nil;
  }

  CGVirtualDisplaySettings* settings = [[CGVirtualDisplaySettings alloc] init];
  settings.hiDPI = hiDPI;
  if (@available(macos 11.0, *)) {
    settings.rotation = 0;
  }
  CGVirtualDisplayMode* mode =
      [[CGVirtualDisplayMode alloc] initWithWidth:(hiDPI ? width / 2 : width)
                                           height:(hiDPI ? height / 2 : height)
                                      refreshRate:60];
  settings.modes = @[ mode ];
  if (![display applySettings:settings]) {
    LOG(ERROR) << __func__ << " - CGVirtualDisplay applySettings failed";
  }
  return display;
}

// This method detects whether the local machine is running headless.
// Typically returns true when the session is curtained or if there are no
// physical monitors attached.  In those two scenarios, the online display will
// be marked as virtual.
bool IsRunningHeadless() {
  // Most machines will have < 4 displays but a larger upper bound won't hurt.
  constexpr UInt32 kMaxDisplaysToQuery = 32;
  // 0x76697274 is a 4CC value for 'virt' which indicates the display is
  // virtual.
  constexpr CGDirectDisplayID kVirtualDisplayID = 0x76697274;

  CGDirectDisplayID online_displays[kMaxDisplaysToQuery];
  UInt32 online_display_count = 0;
  CGError return_code = CGGetOnlineDisplayList(
      kMaxDisplaysToQuery, online_displays, &online_display_count);
  if (return_code != kCGErrorSuccess) {
    LOG(ERROR) << __func__
               << " - CGGetOnlineDisplayList() failed: " << return_code << ".";
    // If this fails, assume machine is headless to err on the side of caution.
    return true;
  }

  bool is_running_headless = true;
  for (UInt32 i = 0; i < online_display_count; i++) {
    if (CGDisplayModelNumber(online_displays[i]) != kVirtualDisplayID) {
      // At least one monitor is attached so the machine is not headless.
      is_running_headless = false;
      break;
    }
  }

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << __func__ << " - Is running headless: " << is_running_headless
            << ". Online display count: " << online_display_count << ".";

  return is_running_headless;
}

// Observer for display metrics change notifications.
class DisplayMetricsChangeObserver : public display::DisplayObserver {
 public:
  DisplayMetricsChangeObserver(int64_t display_id,
                               const gfx::Size& size,
                               uint32_t expected_changed_metrics)
      : display_id_(display_id),
        size_(size),
        expected_changed_metrics_(expected_changed_metrics) {
    display::Screen::GetScreen()->AddObserver(this);
  }
  ~DisplayMetricsChangeObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  DisplayMetricsChangeObserver(const DisplayMetricsChangeObserver&) = delete;
  DisplayMetricsChangeObserver& operator=(const DisplayMetricsChangeObserver&) =
      delete;

  // Runs a loop until the display metrics change is seen (unless one has
  // already been observed, in which case it returns immediately).
  void Wait() {
    if (observed_change_)
      return;

    run_loop_.Run();
  }

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (!(display.id() == display_id_ && display.size() == size_ &&
          (changed_metrics & expected_changed_metrics_))) {
      return;
    }

    observed_change_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }
  void OnDisplayAdded(const display::Display& new_display) override {}
  void OnDisplayRemoved(const display::Display& old_display) override {}

  const int64_t display_id_;
  const gfx::Size size_;
  const uint32_t expected_changed_metrics_;

  bool observed_change_ = false;
  base::RunLoop run_loop_;
};

void EnsureDisplayWithResolution(CGDirectDisplayID display_id,
                                 const gfx::Size& size) {
  base::apple::ScopedCFTypeRef<CGDisplayModeRef> current_display_mode(
      CGDisplayCopyDisplayMode(display_id));
  if (gfx::Size(CGDisplayModeGetWidth(current_display_mode.get()),
                CGDisplayModeGetHeight(current_display_mode.get())) == size) {
    return;
  }

  base::apple::ScopedCFTypeRef<CFArrayRef> display_modes(
      CGDisplayCopyAllDisplayModes(display_id, nullptr));
  DCHECK(display_modes);

  CGDisplayModeRef preferred_display_mode = nullptr;
  for (CFIndex i = 0; i < CFArrayGetCount(display_modes.get()); ++i) {
    CGDisplayModeRef display_mode =
        (CGDisplayModeRef)CFArrayGetValueAtIndex(display_modes.get(), i);
    if (gfx::Size(CGDisplayModeGetWidth(display_mode),
                  CGDisplayModeGetHeight(display_mode)) == size) {
      preferred_display_mode = display_mode;
      break;
    }
  }
  DCHECK(preferred_display_mode);

  uint32_t expected_changed_metrics =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  DisplayMetricsChangeObserver display_metrics_change_observer(
      display_id, size, expected_changed_metrics);

  // This operation is always synchronous. The function doesn’t return until the
  // mode switch is complete.
  CGError result =
      CGDisplaySetDisplayMode(display_id, preferred_display_mode, nullptr);
  DCHECK_EQ(result, kCGErrorSuccess);

  // Wait for `display::Screen` and `display::Display` structures to be updated.
  display_metrics_change_observer.Wait();
}

}  // namespace

namespace display::test {

struct DisplayParams {
  DisplayParams(int width,
                int height,
                int ppi,
                bool hiDPI,
                std::string description)
      : width(width),
        height(height),
        ppi(ppi),
        hiDPI(hiDPI),
        description(base::SysUTF8ToNSString(description)) {}

  bool IsValid() const {
    return width > 0 && height > 0 && ppi > 0 && description.length > 0;
  }

  int width;
  int height;
  int ppi;
  BOOL hiDPI;
  NSString* __strong description;
};

VirtualDisplayMacUtil::VirtualDisplayMacUtil() {
  display::Screen::GetScreen()->AddObserver(this);
}

VirtualDisplayMacUtil::~VirtualDisplayMacUtil() {
  RemoveAllDisplays();
  display::Screen::GetScreen()->RemoveObserver(this);
}

int64_t VirtualDisplayMacUtil::AddDisplay(int64_t display_id,
                                          const DisplayParams& display_params) {
  DCHECK(display_params.IsValid());

  NSString* display_name =
      [NSString stringWithFormat:@"Virtual Display #%lld", display_id];
  CGVirtualDisplay* display = CreateVirtualDisplay(
      display_params.width, display_params.height, display_params.ppi,
      display_params.hiDPI, display_name, display_id);
  DCHECK(display);

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << display_id
            << ". CreateVirtualDisplay success.";

  int64_t id = display.displayID;
  DCHECK_NE(id, 0u);

  WaitForDisplay(id, /*added=*/true);

  EnsureDisplayWithResolution(
      id, gfx::Size(display_params.width, display_params.height));

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << display_id << "(" << id
            << "). WaitForDisplay success.";

  DCHECK(!g_display_map[id]);
  g_display_map[id] = display;

  return id;
}

void VirtualDisplayMacUtil::RemoveDisplay(int64_t display_id) {
  auto it = g_display_map.find(display_id);
  DCHECK(it != g_display_map.end());

  // The first display removal has known flaky timeouts if removed
  // individually. Remove another display simultaneously during the first
  // display removal.
  // TODO(crbug.com/1126278): Resolve this defect in a more hermetic manner.
  if (g_need_display_removal_workaround) {
    const int64_t tmp_display_id = AddDisplay(0, k1920x1080);
    auto tmp_it = g_display_map.find(tmp_display_id);
    DCHECK(tmp_it != g_display_map.end());

    waiting_for_ids_.insert(display_id);
    waiting_for_ids_.insert(tmp_display_id);

    g_display_map.erase(it);
    g_display_map.erase(tmp_it);

    g_need_display_removal_workaround = false;

    StartWaiting();

    return;
  }

  g_display_map.erase(it);

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << display_id << ". Erase success.";

  WaitForDisplay(display_id, /*added=*/false);

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << display_id << ". WaitForDisplay success.";
}

// static
bool VirtualDisplayMacUtil::IsAPIAvailable() {
  bool is_api_available = false;
  // The underlying API is only available on macos 10.14 or higher.
  // TODO(crbug.com/1126278): enable support on 10.15.
  if (@available(macos 11.0, *)) {
    is_api_available = true;
  }
  if (!is_api_available) {
    return false;
  }

  // Only support non-headless bots now. Some odd behavior happened when
  // enable on headless bots. See
  // https://ci.chromium.org/ui/p/chromium/builders/try/mac-rel/1058024/test-results
  // for details.
  // TODO(crbug.com/1126278): Remove this restriction when support headless
  // environment.
  if (IsRunningHeadless()) {
    return false;
  }

  return true;
}

// Predefined display configurations from
// https://en.wikipedia.org/wiki/Graphics_display_resolution and
// https://www.theverge.com/tldr/2016/3/21/11278192/apple-iphone-ipad-screen-sizes-pixels-density-so-many-choices.
const DisplayParams VirtualDisplayMacUtil::k6016x3384 =
    DisplayParams(6016, 3384, 218, true, "Apple Pro Display XDR");
const DisplayParams VirtualDisplayMacUtil::k5120x2880 =
    DisplayParams(5120, 2880, 218, true, "27-inch iMac with Retina 5K display");
const DisplayParams VirtualDisplayMacUtil::k4096x2304 =
    DisplayParams(4096,
                  2304,
                  219,
                  true,
                  "21.5-inch iMac with Retina 4K display");
const DisplayParams VirtualDisplayMacUtil::k3840x2400 =
    DisplayParams(3840, 2400, 200, true, "WQUXGA");
const DisplayParams VirtualDisplayMacUtil::k3840x2160 =
    DisplayParams(3840, 2160, 200, true, "UHD");
const DisplayParams VirtualDisplayMacUtil::k3840x1600 =
    DisplayParams(3840, 1600, 200, true, "WQHD+, UW-QHD+");
const DisplayParams VirtualDisplayMacUtil::k3840x1080 =
    DisplayParams(3840, 1080, 200, true, "DFHD");
const DisplayParams VirtualDisplayMacUtil::k3072x1920 =
    DisplayParams(3072,
                  1920,
                  226,
                  true,
                  "16-inch MacBook Pro with Retina display");
const DisplayParams VirtualDisplayMacUtil::k2880x1800 =
    DisplayParams(2880,
                  1800,
                  220,
                  true,
                  "15.4-inch MacBook Pro with Retina display");
const DisplayParams VirtualDisplayMacUtil::k2560x1600 =
    DisplayParams(2560,
                  1600,
                  227,
                  true,
                  "WQXGA, 13.3-inch MacBook Pro with Retina display");
const DisplayParams VirtualDisplayMacUtil::k2560x1440 =
    DisplayParams(2560, 1440, 109, false, "27-inch Apple Thunderbolt display");
const DisplayParams VirtualDisplayMacUtil::k2304x1440 =
    DisplayParams(2304, 1440, 226, true, "12-inch MacBook with Retina display");
const DisplayParams VirtualDisplayMacUtil::k2048x1536 =
    DisplayParams(2048, 1536, 150, false, "QXGA");
const DisplayParams VirtualDisplayMacUtil::k2048x1152 =
    DisplayParams(2048, 1152, 150, false, "QWXGA");
const DisplayParams VirtualDisplayMacUtil::k1920x1200 =
    DisplayParams(1920, 1200, 150, false, "WUXGA");
const DisplayParams VirtualDisplayMacUtil::k1600x1200 =
    DisplayParams(1600, 1200, 125, false, "UXGA");
const DisplayParams VirtualDisplayMacUtil::k1920x1080 =
    DisplayParams(1920, 1080, 102, false, "HD, 21.5-inch iMac");
const DisplayParams VirtualDisplayMacUtil::k1680x1050 =
    DisplayParams(1680,
                  1050,
                  99,
                  false,
                  "WSXGA+, Apple Cinema Display (20-inch), 20-inch iMac");
const DisplayParams VirtualDisplayMacUtil::k1440x900 =
    DisplayParams(1440, 900, 127, false, "WXGA+, 13.3-inch MacBook Air");
const DisplayParams VirtualDisplayMacUtil::k1400x1050 =
    DisplayParams(1400, 1050, 125, false, "SXGA+");
const DisplayParams VirtualDisplayMacUtil::k1366x768 =
    DisplayParams(1366, 768, 135, false, "11.6-inch MacBook Air");
const DisplayParams VirtualDisplayMacUtil::k1280x1024 =
    DisplayParams(1280, 1024, 100, false, "SXGA");
const DisplayParams VirtualDisplayMacUtil::k1280x1800 =
    DisplayParams(1280, 800, 113, false, "13.3-inch MacBook Pro");

VirtualDisplayMacUtil::DisplaySleepBlocker::DisplaySleepBlocker() {
  IOReturn result = IOPMAssertionCreateWithName(
      kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
      CFSTR("DisplaySleepBlocker"), &assertion_id_);
  DCHECK_EQ(result, kIOReturnSuccess);
}

VirtualDisplayMacUtil::DisplaySleepBlocker::~DisplaySleepBlocker() {
  IOReturn result = IOPMAssertionRelease(assertion_id_);
  DCHECK_EQ(result, kIOReturnSuccess);
}

void VirtualDisplayMacUtil::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << display.id()
            << ", changed_metrics: " << changed_metrics << ".";
}

void VirtualDisplayMacUtil::OnDisplayAdded(
    const display::Display& new_display) {
  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << new_display.id() << ".";

  OnDisplayAddedOrRemoved(new_display.id());
}

void VirtualDisplayMacUtil::OnDisplayRemoved(
    const display::Display& old_display) {
  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display id: " << old_display.id() << ".";

  OnDisplayAddedOrRemoved(old_display.id());
}

void VirtualDisplayMacUtil::OnDisplayAddedOrRemoved(int64_t id) {
  if (!waiting_for_ids_.count(id)) {
    // TODO(crbug.com/1126278): Please remove this log or replace it with
    // [D]CHECK() ASAP when the TEST is stable.
    LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
              << " - unexpected display id: " << id << ".";

    return;
  }

  waiting_for_ids_.erase(id);
  if (!waiting_for_ids_.empty()) {
    return;
  }

  StopWaiting();
}

void VirtualDisplayMacUtil::RemoveAllDisplays() {
  int display_count = g_display_map.size();

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__
            << " - display count: " << display_count << ".";

  if (!display_count) {
    return;
  }

  if (display_count == 1 && g_need_display_removal_workaround) {
    RemoveDisplay(g_display_map.begin()->first);
    return;
  }

  for (const auto& [id, display] : g_display_map) {
    waiting_for_ids_.insert(id);
  }

  g_display_map.clear();

  StartWaiting();
}

void VirtualDisplayMacUtil::WaitForDisplay(int64_t id, bool added) {
  display::Display d;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d) == added) {
    return;
  }

  waiting_for_ids_.insert(id);

  // TODO(crbug.com/1126278): Please remove this log or replace it with
  // [D]CHECK() ASAP when the TEST is stable.
  LOG(INFO) << "VirtualDisplayMacUtil::" << __func__ << " - display id: " << id
            << "(added: " << added << "). Start waiting.";

  StartWaiting();
}

void VirtualDisplayMacUtil::StartWaiting() {
  DCHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void VirtualDisplayMacUtil::StopWaiting() {
  DCHECK(run_loop_);
  run_loop_->Quit();
}

}  // namespace display::test
