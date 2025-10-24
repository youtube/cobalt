/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCNetworkMonitor+Private.h"

#import <Network/Network.h>

#import "base/RTCLogging.h"
#import "helpers/RTCDispatcher+Private.h"

#include "rtc_base/string_utils.h"

namespace {

webrtc::AdapterType AdapterTypeFromInterfaceType(
    nw_interface_type_t interfaceType) {
  webrtc::AdapterType adapterType = webrtc::ADAPTER_TYPE_UNKNOWN;
  switch (interfaceType) {
    case nw_interface_type_other:
      adapterType = webrtc::ADAPTER_TYPE_UNKNOWN;
      break;
    case nw_interface_type_wifi:
      adapterType = webrtc::ADAPTER_TYPE_WIFI;
      break;
    case nw_interface_type_cellular:
      adapterType = webrtc::ADAPTER_TYPE_CELLULAR;
      break;
    case nw_interface_type_wired:
      adapterType = webrtc::ADAPTER_TYPE_ETHERNET;
      break;
    case nw_interface_type_loopback:
      adapterType = webrtc::ADAPTER_TYPE_LOOPBACK;
      break;
    default:
      adapterType = webrtc::ADAPTER_TYPE_UNKNOWN;
      break;
  }
  return adapterType;
}

}  // namespace

@implementation RTCNetworkMonitor {
  webrtc::NetworkMonitorObserver *_observer;
  nw_path_monitor_t _pathMonitor;
  dispatch_queue_t _monitorQueue;
}

- (instancetype)initWithObserver:(webrtc::NetworkMonitorObserver *)observer {
  RTC_DCHECK(observer);
  self = [super init];
  if (self) {
    _observer = observer;
    if (@available(iOS 12, *)) {
      _pathMonitor = nw_path_monitor_create();
      if (_pathMonitor == nil) {
        RTCLog(@"nw_path_monitor_create failed.");
        return nil;
      }
      RTCLog(@"NW path monitor created.");
      __weak RTCNetworkMonitor *weakSelf = self;
      nw_path_monitor_set_update_handler(_pathMonitor, ^(nw_path_t path) {
        RTCNetworkMonitor *strongSelf = weakSelf;
        if (strongSelf == nil) {
          return;
        }
        RTCLog(@"NW path monitor: updated.");
        nw_path_status_t status = nw_path_get_status(path);
        if (status == nw_path_status_invalid) {
          RTCLog(@"NW path monitor status: invalid.");
        } else if (status == nw_path_status_unsatisfied) {
          RTCLog(@"NW path monitor status: unsatisfied.");
        } else if (status == nw_path_status_satisfied) {
          RTCLog(@"NW path monitor status: satisfied.");
        } else if (status == nw_path_status_satisfiable) {
          RTCLog(@"NW path monitor status: satisfiable.");
        }
        std::map<std::string, webrtc::AdapterType, webrtc::AbslStringViewCmp>
            owned_map;
        auto map = &owned_map;  // Capture raw pointer for Objective-C block
        nw_path_enumerate_interfaces(path, ^(nw_interface_t interface) {
          const char *name = nw_interface_get_name(interface);
          nw_interface_type_t interfaceType = nw_interface_get_type(interface);
          RTCLog(@"NW path monitor available interface: %s", name);
          webrtc::AdapterType adapterType =
              AdapterTypeFromInterfaceType(interfaceType);
          map->emplace(name, adapterType);
          return true;
        });
        @synchronized(strongSelf) {
          webrtc::NetworkMonitorObserver *strongObserver =
              strongSelf->_observer;
          if (strongObserver) {
            strongObserver->OnPathUpdate(std::move(owned_map));
          }
        }
      });
      nw_path_monitor_set_queue(
          _pathMonitor,
          [RTC_OBJC_TYPE(RTCDispatcher)
              dispatchQueueForType:RTCDispatcherTypeNetworkMonitor]);
      nw_path_monitor_start(_pathMonitor);
    }
  }
  return self;
}

- (void)cancel {
  if (@available(iOS 12, *)) {
    nw_path_monitor_cancel(_pathMonitor);
  }
}
- (void)stop {
  [self cancel];
  @synchronized(self) {
    _observer = nil;
  }
}

- (void)dealloc {
  [self cancel];
}

@end
