// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/access_code_cast/access_code_cast_integration_browsertest.h"

#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/json/values_util.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

using testing::_;
namespace media_router {

namespace {
const char kEndpointResponseSuccess[] =
    R"({
      "device": {
        "displayName": "test_device",
        "id": "1234",
        "deviceCapabilities": {
          "videoOut": true,
          "videoIn": true,
          "audioOut": true,
          "audioIn": true,
          "devMode": true
        },
        "networkInfo": {
          "hostName": "GoogleNet",
          "port": "666",
          "ipV4Address": "192.0.2.146",
          "ipV6Address": "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
        }
      }
    })";
}  // namespace

class AccessCodeCastSinkServiceBrowserTest
    : public AccessCodeCastIntegrationBrowserTest {};

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest,
                       PRE_InstantExpiration) {
  // This pre test adds a device successfully to the browser. The next test
  // then ensures the devices was not saved when the browsertest starts up
  // again.

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  EnableAccessCodeCasting();

  SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel::kSync,
                                      browser()->profile());

  auto* dialog_contents = ShowDialog();
  SetAccessCode("abcdef", dialog_contents);
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>",
      MediaSource::ForTab(
          sessions::SessionTabHelper::IdForTab(web_contents()).id())
          .id(),
      web_contents());

  PressSubmitAndWaitForClose(dialog_contents);

  // Simulate the route opening and then ending. The device should expire
  // once the route ends.
  MediaRoute media_route_cast = CreateRouteForTesting("cast:<1234>");
  UpdateRoutes({media_route_cast});

  // Let AccessCodeCastSinkService use `task_runner()` so that we can advance
  // mock clock.
  SetAccessCodeCastSinkServiceTaskRunner();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), DisconnectAndRemoveSink(_));
  UpdateRoutes({});
  task_runner()->FastForwardBy(AccessCodeCastSinkService::kExpirationDelay);

  if (!IsAccessCodeCastLacrosSyncEnabled()) {
    // When devices are in sync between Lacros and Ash, Lacros won't remove
    // devices from the pref service.
    // The device should not be stored in the pref service and not in the media
    // router.
    EXPECT_FALSE(HasSinkInDevicesDict("cast:<1234>"));
  }
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest,
                       InstantExpiration) {
  if (IsAccessCodeCastLacrosSyncEnabled()) {
    GTEST_SKIP();
  }

  // This test is run after an instant expiration device was successfully
  // added to the browser. Upon restart it should not exists in prefs nor
  // should it be added to the media router.
  EXPECT_FALSE(HasSinkInDevicesDict("cast:<1234>"));

  base::RunLoop run_loop;
  mock_cast_media_sink_service_impl()
      ->task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                         base::Unretained(mock_cast_media_sink_service_impl()),
                         "cast:<1234>"),
          base::BindOnce(&AccessCodeCastIntegrationBrowserTest::
                             ExpectMediaRouterHasNoSinks,
                         weak_ptr_factory_.GetWeakPtr(),
                         run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest, PRE_SavedDevice) {
  // This pre test adds a device successfully to the browser. The next test then
  // ensures the devices was saved when the browsertest starts up again.

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  EnableAccessCodeCasting();

  SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel::kSync,
                                      browser()->profile());

  // Set the saved devices pref value.
  browser()->profile()->GetPrefs()->Set(
      prefs::kAccessCodeCastDeviceDuration,
      base::Value(static_cast<int>(base::Hours(10).InSeconds())));

  auto* dialog_contents = ShowDialog();
  SetAccessCode("abcdef", dialog_contents);
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>",
      MediaSource::ForTab(
          sessions::SessionTabHelper::IdForTab(web_contents()).id())
          .id(),
      web_contents());

  PressSubmitAndWaitForClose(dialog_contents);

  // Simulate the route opening and then ending. The device should NOT expire
  // once the route ends.
  MediaRoute media_route_cast = CreateRouteForTesting("cast:<1234>");
  UpdateRoutes({media_route_cast});

  // Let AccessCodeCastSinkService to use `task_runner()` so that we can advance
  // mock clock.
  SetAccessCodeCastSinkServiceTaskRunner();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), DisconnectAndRemoveSink(_))
      .Times(0);
  UpdateRoutes({});
  task_runner()->FastForwardBy(AccessCodeCastSinkService::kExpirationDelay);

  // The device should be stored in the pref service and still in the media
  // router.
  EXPECT_TRUE(HasSinkInDevicesDict("cast:<1234>"));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest, SavedDevice) {
  // This test is run after a saved device was successfully added to the
  // browser. Upon restart it should exists in prefs && it should be added
  // to the media router.
  AddScreenplayTag(AccessCodeCastIntegrationBrowserTest::
                       kAccessCodeCastSavedDeviceScreenplayTag);

  EXPECT_TRUE(HasSinkInDevicesDict("cast:<1234>"));

  base::RunLoop run_loop;
  mock_cast_media_sink_service_impl()
      ->task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                         base::Unretained(mock_cast_media_sink_service_impl()),
                         "cast:<1234>"),
          base::BindOnce(
              &AccessCodeCastIntegrationBrowserTest::ExpectMediaRouterHasSink,
              weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();

  // Verify that the saved devices sink added time isn't reset after it has been
  // successfully opened and exists in the media router.
  EXPECT_EQ(GetDeviceAddedTimeFromDict("cast:<1234>").value(),
            device_added_time());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest,
                       OnAccessCodeCastDevicesChanged) {
  if (!IsAccessCodeCastLacrosSyncEnabled()) {
    GTEST_SKIP() << "Skipping as the prefs are not available in the "
                    "current version of Ash";
  }

  // Set prefs value.
  auto cast_sink = CreateCastSink(1);
  base::RunLoop run_loop;
  MediaSinkInternal actual_cast_sink;
  base::Value::Dict devices_dict;
  devices_dict.Set(cast_sink.id(),
                   CreateValueDictFromMediaSinkInternal(cast_sink));
  base::Value::Dict device_added_time_dict;
  device_added_time_dict.Set(cast_sink.id(),
                             base::TimeToValue(base::Time::Now()));

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), HasSink(cast_sink.id()));
  EXPECT_CALL(*mock_cast_media_sink_service_impl(), OpenChannel)
      .WillOnce(testing::WithArg<0>([&](const MediaSinkInternal& sink) {
        actual_cast_sink = sink;
        run_loop.Quit();
      }));

  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();
  prefs->SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
                 base::Value(std::move(devices_dict)), base::DoNothing());
  prefs->SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
                 base::Value(std::move(device_added_time_dict)),
                 base::DoNothing());

  // Wait for the AccessCodeCastSinkService to be notified and add new sink to
  // the Media Router.
  run_loop.Run();
  EXPECT_EQ(cast_sink.id(), actual_cast_sink.id());
}
#endif

}  // namespace media_router
