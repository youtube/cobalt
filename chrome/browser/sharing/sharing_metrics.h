// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_METRICS_H_
#define CHROME_BROWSER_SHARING_SHARING_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"

enum class SharingDeviceRegistrationResult;

// The types of dialogs that can be shown for sharing features.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingDialogType" in src/tools/metrics/histograms/enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sharing
enum class SharingDialogType {
  kDialogWithDevicesMaybeApps = 0,
  kDialogWithoutDevicesWithApp = 1,
  kEducationalDialog = 2,
  kErrorDialog = 3,
  kMaxValue = kErrorDialog,
};

// These histogram suffixes must match the ones in Sharing{feature}Ui
// defined in histograms.xml.
const char kSharingUiContextMenu[] = "ContextMenu";
const char kSharingUiDialog[] = "Dialog";

// Maps SharingSendMessageResult enums to strings used as histogram suffixes.
// Keep in sync with "SharingSendMessageResult" in histograms.xml.
std::string SharingSendMessageResultToString(SharingSendMessageResult result);

// Maps PayloadCase enums to MessageType enums.
chrome_browser_sharing::MessageType SharingPayloadCaseToMessageType(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

// Maps MessageType enums to strings used as histogram suffixes. Keep in sync
// with "SharingMessage" in histograms.xml.
const std::string& SharingMessageTypeToString(
    chrome_browser_sharing::MessageType message_type);

// Generates trace ids for async traces in the "sharing" category.
int GenerateSharingTraceId();

// Logs the |payload_case| to UMA. This should be called when a SharingMessage
// is received.
void LogSharingMessageReceived(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

// Logs the |result| to UMA. This should be called after attempting register
// Sharing.
void LogSharingRegistrationResult(SharingDeviceRegistrationResult result);

// Logs the |result| to UMA. This should be called after attempting un-register
// Sharing.
void LogSharingUnregistrationResult(SharingDeviceRegistrationResult result);

// Logs the number of available devices that are about to be shown in a UI for
// picking a device to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml use the constants defined
// in this file for that.
// TODO(yasmo): Change histogram_suffix to be an enum type.
void LogSharingDevicesToShow(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml - use the constants defined
// in this file for that.
void LogSharingAppsToShow(SharingFeatureName feature,
                          const char* histogram_suffix,
                          int count);

// Logs the |index| of the user selection for sharing feature. |index_type| is
// the type of selection made, either "Device" or "App". The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml - use the constants defined in
// this file for that.
enum class SharingIndexType {
  kDevice,
  kApp,
};
void LogSharingSelectedIndex(
    SharingFeatureName feature,
    const char* histogram_suffix,
    int index,
    SharingIndexType index_type = SharingIndexType::kDevice);

// Logs to UMA the time from sending a FCM message from the Sharing service
// until an ack message is received for it.
void LogSharingMessageAckTime(chrome_browser_sharing::MessageType message_type,
                              SharingDevicePlatform receiver_device_platform,
                              SharingChannelType channel_type,
                              base::TimeDelta time);

// Logs to UMA the time from receiving a SharingMessage to sending
// back an ack.
void LogSharingMessageHandlerTime(
    chrome_browser_sharing::MessageType message_type,
    base::TimeDelta time_taken);

// Logs to UMA the |type| of dialog shown for sharing feature.
void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type);

// Logs to UMA result of sending a SharingMessage. This should not be called for
// sending ack messages.
void LogSendSharingMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform receiver_device_platform,
    SharingChannelType channel_type,
    base::TimeDelta receiver_pulse_interval,
    SharingSendMessageResult result);

// Logs to UMA result of sending an ack of a SharingMessage.
void LogSendSharingAckMessageResult(
    chrome_browser_sharing::MessageType message_type,
    SharingDevicePlatform ack_receiver_device_type,
    SharingChannelType channel_type,
    SharingSendMessageResult result);

// Logs to UMA the size of the selected text for Shared Clipboard.
void LogSharedClipboardSelectedTextSize(size_t text_size);

// Logs to UMA the result of handling a Remote Copy message.
void LogRemoteCopyHandleMessageResult(RemoteCopyHandleMessageResult result);

// Logs to UMA the size of the received text for Remote Copy.
void LogRemoteCopyReceivedTextSize(size_t size);

// Logs to UMA the size of the received image (before decoding) for Remote Copy.
void LogRemoteCopyReceivedImageSizeBeforeDecode(size_t size);

// Logs to UMA the size of the received image (after decoding) for Remote Copy.
void LogRemoteCopyReceivedImageSizeAfterDecode(size_t size);

// Logs to UMA the status code of an image load request for Remote Copy.
void LogRemoteCopyLoadImageStatusCode(int code);

// Logs to UMA the time to load an image for Remote Copy.
void LogRemoteCopyLoadImageTime(base::TimeDelta time);

// Logs to UMA the time to decode an image for Remote Copy.
void LogRemoteCopyDecodeImageTime(base::TimeDelta time);

// Logs to UMA the duration of a clipboard write for Remote Copy.
void LogRemoteCopyWriteTime(base::TimeDelta time, bool is_image);

// Logs to UMA the time to detect a clipboard write for Remote Copy.
void LogRemoteCopyWriteDetectionTime(base::TimeDelta time, bool is_image);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_
