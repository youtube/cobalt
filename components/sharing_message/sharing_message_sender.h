// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/sharing_message/sharing_target_device_info.h"

namespace components_sharing_message {
class ResponseMessage;
class SharingMessage;
}  // namespace components_sharing_message

namespace sharing_message {
enum MessageType : int;
}  // namespace sharing_message

namespace syncer {
class LocalDeviceInfoProvider;
}  // namespace syncer

namespace sync_pb {
class UnencryptedSharingMessage;
}  // namespace sync_pb

enum class SharingChannelType;
class SharingFCMSender;
enum class SharingDevicePlatform;
enum class SharingSendMessageResult;

class SharingMessageSender {
 public:
  using ResponseCallback = base::OnceCallback<void(
      SharingSendMessageResult,
      std::unique_ptr<components_sharing_message::ResponseMessage>)>;

  // Delegate class used to swap the actual message sending implementation.
  class SendMessageDelegate {
   public:
    using SendMessageCallback =
        base::OnceCallback<void(SharingSendMessageResult result,
                                std::optional<std::string> message_id,
                                SharingChannelType channel_type)>;
    virtual ~SendMessageDelegate() = default;

    virtual void DoSendMessageToDevice(
        const SharingTargetDeviceInfo& device,
        base::TimeDelta time_to_live,
        components_sharing_message::SharingMessage message,
        SendMessageCallback callback) = 0;

    virtual void DoSendUnencryptedMessageToDevice(
        const SharingTargetDeviceInfo& device,
        sync_pb::UnencryptedSharingMessage message,
        SendMessageCallback callback) = 0;
  };

  // Delegate type used to send a message.
  enum class DelegateType { kFCM, kWebRtc, kIOSPush };

  SharingMessageSender(
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  SharingMessageSender(const SharingMessageSender&) = delete;
  SharingMessageSender& operator=(const SharingMessageSender&) = delete;
  virtual ~SharingMessageSender();

  virtual base::OnceClosure SendMessageToDevice(
      const SharingTargetDeviceInfo& device,
      base::TimeDelta response_timeout,
      components_sharing_message::SharingMessage message,
      DelegateType delegate_type,
      ResponseCallback callback);

  virtual base::OnceClosure SendUnencryptedMessageToDevice(
      const SharingTargetDeviceInfo& device,
      sync_pb::UnencryptedSharingMessage message,
      DelegateType delegate_type,
      ResponseCallback callback);

  virtual void OnAckReceived(
      const std::string& message_id,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  // Registers the given |delegate| to send messages when SendMessageToDevice is
  // called with |type|.
  void RegisterSendDelegate(DelegateType type,
                            std::unique_ptr<SendMessageDelegate> delegate);

  // Returns SharingFCMSender for testing.
  SharingFCMSender* GetFCMSenderForTesting() const;

 private:
  struct SentMessageMetadata {
    SentMessageMetadata(ResponseCallback callback,
                        base::TimeTicks timestamp,
                        sharing_message::MessageType type,
                        SharingDevicePlatform receiver_device_platform,
                        int trace_id,
                        SharingChannelType channel_type,
                        base::TimeDelta receiver_pulse_interval);
    SentMessageMetadata(SentMessageMetadata&& other);
    SentMessageMetadata& operator=(SentMessageMetadata&& other);
    ~SentMessageMetadata();

    ResponseCallback callback;
    base::TimeTicks timestamp;
    sharing_message::MessageType type;
    SharingDevicePlatform receiver_device_platform;
    int trace_id;
    SharingChannelType channel_type;
    base::TimeDelta receiver_pulse_interval;
  };

  void OnMessageSent(const std::string& message_guid,
                     sharing_message::MessageType message_type,
                     SharingSendMessageResult result,
                     std::optional<std::string> message_id,
                     SharingChannelType channel_type);

  void InvokeSendMessageCallback(
      const std::string& message_guid,
      SharingSendMessageResult result,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  SendMessageDelegate* MaybeGetSendMessageDelegate(
      const SharingTargetDeviceInfo& device,
      sharing_message::MessageType message_type,
      int trace_id,
      const std::string& message_guid,
      DelegateType delegate_type);

  raw_ptr<syncer::LocalDeviceInfoProvider> local_device_info_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Map of random GUID to SentMessageMetadata.
  std::map<std::string, SentMessageMetadata> message_metadata_;
  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;
  // Map of FCM message_id to received ACK response messages.
  std::map<std::string,
           std::unique_ptr<components_sharing_message::ResponseMessage>>
      cached_ack_response_messages_;

  // Registered delegates to send messages.
  std::map<DelegateType, std::unique_ptr<SendMessageDelegate>> send_delegates_;

  base::WeakPtrFactory<SharingMessageSender> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_SENDER_H_
