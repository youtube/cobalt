// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.view.Surface;
import androidx.test.core.app.ApplicationProvider;
import dev.cobalt.coat.CobaltService.ResponseToClient;
import dev.cobalt.media.VideoSurfaceView;
import dev.cobalt.util.Holder;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;

/** Unit tests for BaseStarboardBridge. */
@RunWith(RobolectricTestRunner.class)
public class BaseStarboardBridgeTest {
  @Rule public final MockitoRule mocks = MockitoJUnit.rule();

  @Mock private BaseStarboardBridge.Natives mockNatives;
  @Mock private VideoSurfaceView.Natives mockVideoNatives;

  private Context context;
  private Holder<Activity> activityHolder;
  private Holder<Service> serviceHolder;
  private BaseStarboardBridge bridge;

  private static class TestLifecycleCobaltService extends CobaltService {
    int startOrResumeCount = 0;
    int suspendCount = 0;
    int stoppedCount = 0;
    int closeCount = 0;
    boolean returnInvalidState = false;

    @Override
    public void beforeStartOrResume() {
      startOrResumeCount++;
    }

    @Override
    public void beforeSuspend() {
      suspendCount++;
    }

    @Override
    public void afterStopped() {
      stoppedCount++;
    }

    @Override
    public ResponseToClient receiveFromClient(byte[] data) {
      ResponseToClient response = new ResponseToClient();
      response.invalidState = returnInvalidState;
      response.data = data;
      return response;
    }

    @Override
    public void close() {
      closeCount++;
    }
  }

  @Before
  public void setUp() {
    BaseStarboardBridge.setNativesForTesting(mockNatives);
    VideoSurfaceView.setNativesForTesting(mockVideoNatives);
    context = ApplicationProvider.getApplicationContext();

    activityHolder = new Holder<>();
    serviceHolder = new Holder<>();
    bridge =
        new BaseStarboardBridge(
            context, activityHolder, serviceHolder, new String[] {"--test"}, "") {};
  }

  @After
  public void tearDown() {
    BaseStarboardBridge.setNativesForTesting(null);
    VideoSurfaceView.setNativesForTesting(null);
    BaseStarboardBridge.setInstanceForTesting(null);
  }

  @Test
  public void openCobaltService_multipleInstancesSameName_storedByHandle() {
    final TestLifecycleCobaltService service1 = new TestLifecycleCobaltService();
    final TestLifecycleCobaltService service2 = new TestLifecycleCobaltService();

    bridge.registerCobaltService(
        new CobaltService.Factory() {
          private int createCount = 0;

          @Override
          public CobaltService createCobaltService(long nativeService) {
            createCount++;
            return createCount == 1 ? service1 : service2;
          }

          @Override
          public String getServiceName() {
            return "testService";
          }
        });

    CobaltService opened1 = bridge.openCobaltService(101L, "testService");
    CobaltService opened2 = bridge.openCobaltService(102L, "testService");

    assertEquals(service1, opened1);
    assertEquals(service2, opened2);
    assertEquals(101L, service1.getNativeService());
    assertEquals(102L, service2.getNativeService());

    // Close only the first instance
    bridge.closeCobaltService(101L);
    assertEquals(1, service1.stoppedCount);
    assertEquals(1, service1.closeCount);
    assertEquals(0, service2.closeCount);

    // Close the second instance
    bridge.closeCobaltService(102L);
    assertEquals(1, service2.stoppedCount);
    assertEquals(1, service2.closeCount);
  }

  @Test
  public void overlappingActivities_gatingBeforeStartAndSuspend() {
    final TestLifecycleCobaltService service = new TestLifecycleCobaltService();
    bridge.registerCobaltService(
        new CobaltService.Factory() {
          @Override
          public CobaltService createCobaltService(long nativeService) {
            return service;
          }

          @Override
          public String getServiceName() {
            return "lifecycleService";
          }
        });

    bridge.openCobaltService(201L, "lifecycleService");

    Activity activity1 = mock(Activity.class);
    Activity activity2 = mock(Activity.class);

    bridge.onActivityCreate(activity1);
    assertTrue(bridge.hasLiveActivities());

    // Activity 1 starts: 0 -> 1 transition
    bridge.onActivityStart(activity1);
    assertTrue(bridge.hasStartedActivities());
    assertEquals(1, service.startOrResumeCount);

    bridge.onActivityCreate(activity2);

    // Activity 2 starts: 1 -> 2 transition (should NOT call beforeStartOrResume again)
    bridge.onActivityStart(activity2);
    assertEquals(1, service.startOrResumeCount);

    // Activity 1 stops: 2 -> 1 transition (should NOT call beforeSuspend)
    bridge.onActivityStop(activity1);
    assertTrue(bridge.hasStartedActivities());
    assertEquals(0, service.suspendCount);

    // Activity 1 destroyed: activity 2 is still live, services must NOT be torn down
    bridge.onActivityDestroy(activity1);
    assertTrue(bridge.hasLiveActivities());
    assertNotNull(bridge.getOpenedCobaltService("lifecycleService"));
    assertEquals(0, service.closeCount);

    // Activity 2 stops: 1 -> 0 transition (should call beforeSuspend)
    bridge.onActivityStop(activity2);
    assertFalse(bridge.hasStartedActivities());
    assertEquals(1, service.suspendCount);

    // Activity 2 destroyed: all activities gone; safety net force-cleans remaining services
    bridge.onActivityDestroy(activity2);
    assertFalse(bridge.hasLiveActivities());
    assertNull(bridge.getOpenedCobaltService("lifecycleService"));
    assertEquals(1, service.closeCount);
  }

  @Test
  public void sendToCobaltService_invalidState_closesServiceByHandle() {
    final TestLifecycleCobaltService service = new TestLifecycleCobaltService();
    service.returnInvalidState = true;

    bridge.registerCobaltService(
        new CobaltService.Factory() {
          @Override
          public CobaltService createCobaltService(long nativeService) {
            return service;
          }

          @Override
          public String getServiceName() {
            return "invalidTestService";
          }
        });

    bridge.openCobaltService(301L, "invalidTestService");
    byte[] response = bridge.sendToCobaltService("invalidTestService", new byte[] {1, 2, 3});

    assertNull(response);
    assertEquals(1, service.closeCount);
    assertNull(bridge.getOpenedCobaltService("invalidTestService"));
  }

  @Test
  public void unregisterCobaltService_removesFactory() {
    bridge.registerCobaltService(
        new CobaltService.Factory() {
          @Override
          public CobaltService createCobaltService(long nativeService) {
            return new TestLifecycleCobaltService();
          }

          @Override
          public String getServiceName() {
            return "removableService";
          }
        });

    assertTrue(bridge.hasCobaltService("removableService"));
    bridge.unregisterCobaltService("removableService");
    assertFalse(bridge.hasCobaltService("removableService"));
  }

  @Test
  public void onVideoSurfaceCreated_setsSurfaceAndNotifiesNative() {
    Surface surface = mock(Surface.class);

    bridge.onVideoSurfaceCreated(surface);

    assertEquals(surface, bridge.getVideoSurface());
    verify(mockVideoNatives).onVideoSurfaceChanged(surface);
  }

  @Test
  public void onVideoSurfaceDestroyed_activeSurface_resetsSurfaceAndNotifiesNull() {
    Surface surface = mock(Surface.class);

    bridge.onVideoSurfaceCreated(surface);
    bridge.onVideoSurfaceDestroyed(surface);

    assertNull(bridge.getVideoSurface());
    verify(mockVideoNatives).onVideoSurfaceChanged(null);
  }

  @Test
  public void onVideoSurfaceDestroyed_staleSurfaceFromPreviousActivity_ignored() {
    Surface surface1 = mock(Surface.class);
    Surface surface2 = mock(Surface.class);

    bridge.onVideoSurfaceCreated(surface1);
    bridge.onVideoSurfaceCreated(surface2);
    assertEquals(surface2, bridge.getVideoSurface());

    // Stale surface destruction should be ignored
    bridge.onVideoSurfaceDestroyed(surface1);

    assertEquals(surface2, bridge.getVideoSurface());
    verify(mockVideoNatives, never()).onVideoSurfaceChanged(null);
  }

  @Test
  public void onActivityDestroy_lastActivityDestroyed_cleansUpVideoSurface() {
    Activity activity = mock(Activity.class);
    Surface surface = mock(Surface.class);

    bridge.onActivityCreate(activity);
    bridge.onVideoSurfaceCreated(surface);
    assertEquals(surface, bridge.getVideoSurface());

    bridge.onActivityDestroy(activity);

    assertNull(bridge.getVideoSurface());
    verify(mockVideoNatives).onVideoSurfaceChanged(null);
  }

  @Test
  public void onActivityDestroy_otherActivityStillLive_preservesActiveVideoSurface() {
    Activity activity1 = mock(Activity.class);
    Activity activity2 = mock(Activity.class);
    Surface surface2 = mock(Surface.class);

    bridge.onActivityCreate(activity1);
    bridge.onActivityCreate(activity2);
    bridge.onVideoSurfaceCreated(surface2);

    bridge.onActivityDestroy(activity1);

    assertEquals(surface2, bridge.getVideoSurface());
    verify(mockVideoNatives, never()).onVideoSurfaceChanged(null);
  }
}
