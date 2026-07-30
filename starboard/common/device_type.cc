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

#include "starboard/common/device_type.h"

namespace starboard {

// Blue-ray Disc Player (BDP).
const char kSystemDeviceTypeBlueRayDiskPlayer[] = "BDP";

// A relatively high-powered TV device used primarily for playing games.
const char kSystemDeviceTypeGameConsole[] = "GAME";

// Over the top (OTT) devices stream content via the Internet over another
// type of network, e.g. cable or satellite.
const char kSystemDeviceTypeOverTheTopBox[] = "OTT";

// Set top boxes (STBs) stream content primarily over cable or satellite.
// Some STBs can also stream OTT content via the Internet.
const char kSystemDeviceTypeSetTopBox[] = "STB";

// A Smart TV is a TV that can directly run applications that stream OTT
// content via the Internet.
const char kSystemDeviceTypeTV[] = "TV";

// An Android TV Device.
const char kSystemDeviceTypeAndroidTV[] = "ATV";

// Desktop PC.
const char kSystemDeviceTypeDesktopPC[] = "DESKTOP";

// A wall video projector.
const char kSystemDeviceTypeVideoProjector[] = "PROJECTOR";

// Such Multimedia Devices (MMD) are virtual assistants with integrated
// displays.
const char kSystemDeviceTypeMultimediaDevices[] = "MMD";

// Such devices are smart computer monitors.
const char kSystemDeviceTypeMonitor[] = "MONITOR";

// Such devices are available inside Automotive vehicles.
const char kSystemDeviceTypeAuto[] = "AUTO";

// Such devices are speakers integrated with OTT features.
const char kSystemDeviceTypeSoundBar[] = "SOUNDBAR";

// Such devices are used in places like Hotels.
const char kSystemDeviceTypeHospitality[] = "HOSPITALITY";

// Unknown device.
const char kSystemDeviceTypeUnknown[] = "UNKNOWN";

}  // namespace starboard
