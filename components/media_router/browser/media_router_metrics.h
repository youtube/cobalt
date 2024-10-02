// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom-forward.h"
#include "components/media_router/common/route_request_result.h"
#include "media/base/container_names.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace media_router {

enum class SinkIconType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When making changes, also update the
// enum list in tools/metrics/histograms/enums.xml to keep it in sync.

// NOTE: For metrics specific to the Media Router component extension, see
// mojo/media_router_mojo_metrics.h.

// This enum is a cartesian product of dialog activation locations and Cast
// modes. Per tools/metrics/histograms/README.md, a multidimensional histogram
// must be flattened into one dimension.
enum class DialogActivationLocationAndCastMode {
  kPinnedIconAndPresentation,
  kPinnedIconAndTabMirror,
  kPinnedIconAndDesktopMirror,
  kPinnedIconAndLocalFile,  // Obsolete.
  // One can start casting from an ephemeral icon by stopping a session, then
  // starting another from the same dialog.
  kEphemeralIconAndPresentation,
  kEphemeralIconAndTabMirror,
  kEphemeralIconAndDesktopMirror,
  kEphemeralIconAndLocalFile,  // Obsolete.
  kContextMenuAndPresentation,
  kContextMenuAndTabMirror,
  kContextMenuAndDesktopMirror,
  kContextMenuAndLocalFile,  // Obsolete.
  kPageAndPresentation,
  kPageAndTabMirror,
  kPageAndDesktopMirror,
  kPageAndLocalFile,  // Obsolete.
  kAppMenuAndPresentation,
  kAppMenuAndTabMirror,
  kAppMenuAndDesktopMirror,
  kAppMenuAndLocalFile,  // Obsolete.
  kSharingHubAndPresentation,
  kSharingHubAndTabMirror,
  kSharingHubAndDesktopMirror,
  kPinnedIconAndRemotePlayback,
  kEphemeralIconAndRemotePlayback,
  kContextMenuAndRemotePlayback,
  kPageAndRemotePlayback,
  kAppMenuAndRemotePlayback,
  kSharingHubAndRemotePlayback,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  kMaxValue = kSharingHubAndRemotePlayback,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Where the user clicked to open the Media Router dialog.
enum class MediaRouterDialogActivationLocation {
  TOOLBAR = 0,
  OVERFLOW_MENU = 1,
  CONTEXTUAL_MENU = 2,
  PAGE = 3,
  APP_MENU = 4,
  SYSTEM_TRAY = 5,
  SHARING_HUB = 6,

  // NOTE: Add entries only immediately above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  TOTAL_COUNT = 7
};

// The possible actions a user can take while interacting with the Media Router
// dialog.
enum class MediaRouterUserAction {
  CHANGE_MODE = 0,
  START_LOCAL = 1,
  STOP_LOCAL = 2,
  CLOSE = 3,
  STATUS_REMOTE = 4,
  REPLACE_LOCAL_ROUTE = 5,
  STOP_REMOTE = 6,

  // Note: Add entries only immediately above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  TOTAL_COUNT = 7
};

enum class PresentationUrlType {
  kOther,
  kCast,            // cast:
  kCastDial,        // cast-dial:
  kCastLegacy,      // URLs that start with |kLegacyCastPresentationUrlPrefix|.
  kDial,            // dial:
  kHttp,            // http:
  kHttps,           // https:
  kRemotePlayback,  // remote-playback:
  // Add new types only immediately above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  kPresentationUrlTypeCount
};

enum class UiType {
  kCastDialog,
  kGlobalMediaControls,
};

enum class MediaRouterAndroidDialogType {
  kRouteController = 0,
  kRouteChooser = 1,
  kMaxValue = kRouteChooser,
};

enum class MediaRouterAndroidDialogAction {
  kTerminateRoute = 0,
  kStartRoute = 1,
  kMaxValue = kStartRoute,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaRouterUserPromptWhenLaunchingCast {
  kPendingUserAuth = 0,
  kUserNotAllowed = 1,

  // Add new types only immediately above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  kMaxValue = kUserNotAllowed,
};

class MediaRouterMetrics {
 public:
  MediaRouterMetrics();
  ~MediaRouterMetrics();

  // UMA histogram names.
  static const char kHistogramCloseLatency[];
  static const char kHistogramIconClickLocation[];
  static const char kHistogramMediaRouterFileFormat[];
  static const char kHistogramMediaRouterFileSize[];
  static const char kHistogramMediaSinkType[];
  static const char kHistogramPresentationUrlType[];
  static const char kHistogramStartLocalLatency[];
  static const char kHistogramStartLocalPosition[];
  static const char kHistogramStartLocalSessionSuccessful[];
  static const char kHistogramStopRoute[];
  static const char kHistogramUiDeviceCount[];
  static const char kHistogramUiDialogActivationLocationAndCastMode[];
  static const char kHistogramUiDialogIconStateAtOpen[];
  static const char kHistogramUiDialogLoadedWithData[];
  static const char kHistogramUiDialogPaint[];
  static const char kHistogramUiFirstAction[];
  static const char kHistogramUiIconStateAtInit[];
  static const char kHistogramUiAndroidDialogType[];
  static const char kHistogramUiAndroidDialogAction[];
  static const char kHistogramUserPromptWhenLaunchingCast[];
  static const char kHistogramPendingUserAuthLatency[];

  // When recording the number of devices shown in UI we record after a delay
  // because discovering devices can take some time after the UI is shown.
  static const base::TimeDelta kDeviceCountMetricDelay;

  // Records where the user clicked to open the Media Router dialog.
  static void RecordMediaRouterDialogActivationLocation(
      MediaRouterDialogActivationLocation activation_location);

  // Records the duration it takes for the Media Router dialog to open and
  // finish painting after a user clicks to open the dialog.
  static void RecordMediaRouterDialogPaint(const base::TimeDelta& delta);

  // Records the duration it takes for the Media Router dialog to load its
  // initial data after a user clicks to open the dialog.
  static void RecordMediaRouterDialogLoaded(const base::TimeDelta& delta);

  // Records the duration it takes from the user opening the Media Router dialog
  // to the user closing the dialog. This is only called if closing the dialog
  // is the first action the user takes.
  static void RecordCloseDialogLatency(const base::TimeDelta& delta);

  // Records the first action the user took after the Media Router dialog
  // opened.
  static void RecordMediaRouterInitialUserAction(MediaRouterUserAction action);

  // Records the format of a cast file.
  static void RecordMediaRouterFileFormat(
      media::container_names::MediaContainerName format);

  // Records the size of a cast file.
  static void RecordMediaRouterFileSize(int64_t size);

  // Records the type of Presentation URL used by a web page.
  static void RecordPresentationUrlType(const GURL& url);

  // Records the type of the sink that media is being Cast to.
  static void RecordMediaSinkType(SinkIconType sink_icon_type);
  static void RecordMediaSinkTypeForGlobalMediaControls(
      SinkIconType sink_icon_type);
  static void RecordMediaSinkTypeForCastDialog(SinkIconType sink_icon_type);

  // Records the number of devices shown in the Cast dialog. The device count
  // may be 0.
  static void RecordDeviceCount(int device_count);

  // Records the index of the device the user has started casting to on the
  // devices list. The index starts at 0.
  static void RecordStartRouteDeviceIndex(int index);

  // Records the time it takes from the Media Router dialog showing at least one
  // device to the user starting to cast. This is called only if casting is the
  // first action taken by the user, aside from selecting the sink to cast to.
  static void RecordStartLocalSessionLatency(const base::TimeDelta& delta);

  // Records whether or not an attempt to start casting was successful.
  static void RecordStartLocalSessionSuccessful(bool success);

  // Records the user stopping a route in the UI.
  static void RecordStopLocalRoute();
  static void RecordStopRemoteRoute();

  // Records whether the toolbar icon is pinned by the user pref / admin policy.
  // Recorded whenever the Cast dialog is opened.
  static void RecordIconStateAtDialogOpen(bool is_pinned);

  // Records whether the toolbar icon is pinned by the user pref / admin policy.
  // Recorded whenever the browser is initialized.
  static void RecordIconStateAtInit(bool is_pinned);

  // Records the outcome of a create route request to a Media Route Provider.
  // This and the following methods that record ResultCode use per-provider
  // histograms.
  static void RecordCreateRouteResultCode(
      mojom::RouteRequestResultCode result_code,
      absl::optional<mojom::MediaRouteProviderId> provider_id = absl::nullopt);

  // Records the outcome of a join route request to a Media Route Provider.
  static void RecordJoinRouteResultCode(
      mojom::RouteRequestResultCode result_code,
      absl::optional<mojom::MediaRouteProviderId> provider_id = absl::nullopt);

  // Records the outcome of a call to terminateRoute() on a Media Route
  // Provider.
  static void RecordMediaRouteProviderTerminateRoute(
      mojom::RouteRequestResultCode result_code,
      absl::optional<mojom::MediaRouteProviderId> provider_id = absl::nullopt);

  // Records the type of the MediaRouter dialog opened. Android only.
  static void RecordMediaRouterAndroidDialogType(
      MediaRouterAndroidDialogType type);

  // Records the action taken on the MediaRouter dialog. Android only.
  static void RecordMediaRouterAndroidDialogAction(
      MediaRouterAndroidDialogAction action);

  // Records the number of times the user was asked to allow casting and the
  // number of times the user didn't allow it
  static void RecordMediaRouterUserPromptWhenLaunchingCast(
      MediaRouterUserPromptWhenLaunchingCast user_prompt);

  // Records the duration it takes between sending cast request and receiving a
  // response of UserPendingAuthorization
  static void RecordMediaRouterPendingUserAuthLatency(
      const base::TimeDelta& delta);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_
