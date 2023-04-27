// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard device type module
//
// Defines Type of the device, e.g. such as "TV", "STB", "OTT".
// Please see Youtube Technical requirements for a full list
// of allowed values.

#ifndef STARBOARD_COMMON_DEVICE_TYPE_H_
#define STARBOARD_COMMON_DEVICE_TYPE_H_

namespace starboard {

// Blue-ray Disc Player (BDP).
extern const char kSystemDeviceTypeBlueRayDiskPlayer[];

// A relatively high-powered TV device used primarily for playing games.
extern const char kSystemDeviceTypeGameConsole[];

// Over the top (OTT) devices stream content via the Internet over another
// type of network, e.g. cable or satellite.
extern const char kSystemDeviceTypeOverTheTopBox[];

// Set top boxes (STBs) stream content primarily over cable or satellite.
// Some STBs can also stream OTT content via the Internet.
extern const char kSystemDeviceTypeSetTopBox[];

// A Smart TV is a TV that can directly run applications that stream OTT
// content via the Internet.
extern const char kSystemDeviceTypeTV[];

// An Android TV Device.
extern const char kSystemDeviceTypeAndroidTV[];

// Desktop PC.
extern const char kSystemDeviceTypeDesktopPC[];

// A wall video projector.
extern const char kSystemDeviceTypeVideoProjector[];

// Such Multimedia Devices (MMD) are virtual assistants with integrated
// displays.
extern const char kSystemDeviceTypeMultimediaDevices[];

// Such devices are smart computer monitors.
extern const char kSystemDeviceTypeMonitor[];

// Such devices are available inside Automotive vehicles.
extern const char kSystemDeviceTypeAuto[];

// Such devices are speakers integrated with OTT features.
extern const char kSystemDeviceTypeSoundBar[];

// Such devices are used in places like Hotels.
extern const char kSystemDeviceTypeHospitality[];

// Unknown device.
extern const char kSystemDeviceTypeUnknown[];

}  // namespace starboard

#endif  // STARBOARD_COMMON_DEVICE_TYPE_H_
