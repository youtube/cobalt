// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/content_constants.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/serialization_format.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {

namespace {

// Unfortunately, we have a layering violation in the runtime API. The runtime
// API is defined at the //extensions layer, but the `MessageSender` type has an
// optional `tabs.Tab` property. This causes issues in type validation because
// when we try to look up the `tabs.Tab` property, it fails (since it's only
// defined in //chrome). This is a "real bug" in that it's a layering violation,
// but it doesn't have real-world implications since right now the only consumer
// of //extensions is //chrome (and thus, the tabs API will always be defined).
// Ignore validation for the affected runtime message-related events.
class RuntimeMessageValidationIgnorer {
 public:
  RuntimeMessageValidationIgnorer()
      : test_handler_(base::BindRepeating(
            &RuntimeMessageValidationIgnorer::HardValidationFailure)) {
    test_handler_.IgnoreSignature("runtime.onMessage");
    test_handler_.IgnoreSignature("runtime.onMessageExternal");
    test_handler_.IgnoreSignature("runtime.onConnect");
  }
  ~RuntimeMessageValidationIgnorer() = default;

 private:
  // Hard-fail on any unexpected validation errors.
  static void HardValidationFailure(const std::string& name,
                                    const std::string& failure) {
    NOTREACHED() << "Unexpected validation failure: " << name << ", "
                 << failure;
  }

  APIResponseValidator::TestHandler test_handler_;
};

}  // namespace

class NativeRendererMessagingServiceTest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  NativeRendererMessagingServiceTest() {}

  NativeRendererMessagingServiceTest(
      const NativeRendererMessagingServiceTest&) = delete;
  NativeRendererMessagingServiceTest& operator=(
      const NativeRendererMessagingServiceTest&) = delete;

  ~NativeRendererMessagingServiceTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    extension_ = ExtensionBuilder("foo").Build();
    RegisterExtension(extension_);

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    script_context_ = CreateScriptContext(context, extension_.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT);
    script_context_->set_url(extension_->url());
    bindings_system()->UpdateBindingsForContext(script_context_);
  }
  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    messaging_service_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  NativeRendererMessagingService* messaging_service() {
    return messaging_service_.get();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<NativeRendererMessagingService> messaging_service_;

  ScriptContext* script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

TEST_F(NativeRendererMessagingServiceTest, ValidateMessagePort) {
  v8::HandleScope handle_scope(isolate());

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false, SerializationFormat::kJson);

  EXPECT_FALSE(
      messaging_service()->HasPortForTesting(script_context(), port_id));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, false));
  messaging_service()->ValidateMessagePort(script_context_set(), port_id,
                                           nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

  messaging_service()->CreatePortForTesting(script_context(), "channel",
                                            port_id);
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  // With a valid port, we shouldn't dispatch a message to close it.
  messaging_service()->ValidateMessagePort(script_context_set(), port_id,
                                           nullptr);
}

TEST_F(NativeRendererMessagingServiceTest, OpenMessagePort) {
  RuntimeMessageValidationIgnorer message_validation_ignorer;

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false, SerializationFormat::kJson);
  EXPECT_FALSE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  const std::string channel_name = "some channel";
  ExtensionMsg_TabConnectionInfo tab_connection_info;
  tab_connection_info.frame_id = 0;
  const int tab_id = 10;
  GURL source_url("http://example.com");
  tab_connection_info.tab = DictionaryBuilder().Set("tabId", tab_id).Build();
  ExtensionMsg_ExternalConnectionInfo external_connection_info;
  external_connection_info.target_id = extension()->id();
  external_connection_info.source_endpoint =
      MessagingEndpoint::ForExtension(extension()->id());
  external_connection_info.source_url = source_url;
  external_connection_info.guest_process_id =
      content::kInvalidChildProcessUniqueId;
  external_connection_info.guest_render_frame_routing_id = 0;

  const char kAddListener[] =
      "(function() {\n"
      "  chrome.runtime.onConnect.addListener(function(port) {\n"
      "    this.eventFired = true;\n"
      "    this.sender = port.sender\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kAddListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessagePort(MSG_ROUTING_NONE, port_id));
  messaging_service()->DispatchOnConnect(
      script_context_set(), port_id, ChannelType::kConnect, channel_name,
      tab_connection_info, external_connection_info, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

  ASSERT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "eventFired"));
  base::Value::Dict expected_sender =
      DictionaryBuilder()
          .Set("frameId", 0)
          .Set("tab", DictionaryBuilder().Set("tabId", tab_id).Build())
          .Set("url", source_url.spec())
          .Set("id", extension()->id())
          .Build();
  EXPECT_EQ(ValueToString(base::Value(std::move(expected_sender))),
            GetStringPropertyFromObject(context->Global(), context, "sender"));
}

TEST_F(NativeRendererMessagingServiceTest, DeliverMessageToPort) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id1(other_context_id, 0, false, SerializationFormat::kJson);
  const PortId port_id2(other_context_id, 1, false, SerializationFormat::kJson);

  gin::Handle<GinPort> port1 = messaging_service()->CreatePortForTesting(
      script_context(), "channel1", port_id1);
  gin::Handle<GinPort> port2 = messaging_service()->CreatePortForTesting(
      script_context(), "channel2", port_id2);
  ASSERT_FALSE(port1.IsEmpty());

  const char kOnMessageListenerTemplate[] =
      "(function(port) {\n"
      "  port.onMessage.addListener((message) => {\n"
      "    this.%s = message;\n"
      "  });\n"
      "})";
  const char kPort1Message[] = "port1Message";
  const char kPort2Message[] = "port2Message";
  {
    v8::Local<v8::Function> add_on_message_listener = FunctionFromString(
        context, base::StringPrintf(kOnMessageListenerTemplate, kPort1Message));
    v8::Local<v8::Value> args[] = {port1.ToV8()};
    RunFunctionOnGlobal(add_on_message_listener, context, std::size(args),
                        args);
  }
  {
    v8::Local<v8::Function> add_on_message_listener = FunctionFromString(
        context, base::StringPrintf(kOnMessageListenerTemplate, kPort2Message));
    v8::Local<v8::Value> args[] = {port2.ToV8()};
    RunFunctionOnGlobal(add_on_message_listener, context, std::size(args),
                        args);
  }

  // We've only added listeners (not dispatched any messages), so neither
  // listener should have been triggered.
  v8::Local<v8::Object> global = context->Global();
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort1Message));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort2Message));

  const char kMessageString[] = R"({"data":"hello"})";
  messaging_service()->DeliverMessage(
      script_context_set(), port_id1,
      Message(kMessageString, SerializationFormat::kJson, false), nullptr);

  // Only port1 should have been notified of the message (ports only receive
  // messages directed to themselves).
  EXPECT_EQ(kMessageString,
            GetStringPropertyFromObject(global, context, kPort1Message));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort2Message));
}

TEST_F(NativeRendererMessagingServiceTest, DisconnectMessagePort) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id1(other_context_id, 0, false, SerializationFormat::kJson);
  const PortId port_id2(other_context_id, 1, false, SerializationFormat::kJson);

  gin::Handle<GinPort> port1 = messaging_service()->CreatePortForTesting(
      script_context(), "channel1", port_id1);
  gin::Handle<GinPort> port2 = messaging_service()->CreatePortForTesting(
      script_context(), "channel2", port_id2);

  const char kOnDisconnectListenerTemplate[] =
      "(function(port) {\n"
      "  port.onDisconnect.addListener(() => {\n"
      "    this.%s = true;\n"
      "  });\n"
      "})";
  const char kPort1Disconnect[] = "port1Disconnect";
  const char kPort2Disconnect[] = "port2Disconnect";
  {
    v8::Local<v8::Function> add_on_disconnect_listener = FunctionFromString(
        context,
        base::StringPrintf(kOnDisconnectListenerTemplate, kPort1Disconnect));
    v8::Local<v8::Value> args[] = {port1.ToV8()};
    RunFunctionOnGlobal(add_on_disconnect_listener, context, std::size(args),
                        args);
  }
  {
    v8::Local<v8::Function> add_on_disconnect_listener = FunctionFromString(
        context,
        base::StringPrintf(kOnDisconnectListenerTemplate, kPort2Disconnect));
    v8::Local<v8::Value> args[] = {port2.ToV8()};
    RunFunctionOnGlobal(add_on_disconnect_listener, context, std::size(args),
                        args);
  }

  v8::Local<v8::Object> global = context->Global();
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort1Disconnect));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort2Disconnect));

  messaging_service()->DispatchOnDisconnect(script_context_set(), port_id1,
                                            std::string(), nullptr);

  EXPECT_EQ("true",
            GetStringPropertyFromObject(global, context, kPort1Disconnect));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(global, context, kPort2Disconnect));
}

TEST_F(NativeRendererMessagingServiceTest, PostMessageFromJS) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false, SerializationFormat::kJson);

  gin::Handle<GinPort> port = messaging_service()->CreatePortForTesting(
      script_context(), "channel", port_id);
  v8::Local<v8::Object> port_object = port.ToV8().As<v8::Object>();

  const char kDispatchMessage[] =
      "(function(port) {\n"
      "  port.postMessage({data: 'hello'});\n"
      "})";
  v8::Local<v8::Function> post_message =
      FunctionFromString(context, kDispatchMessage);
  v8::Local<v8::Value> args[] = {port_object};

  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(
                  port_id, Message(R"({"data":"hello"})",
                                   SerializationFormat::kJson, false)));
  RunFunctionOnGlobal(post_message, context, std::size(args), args);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
}

TEST_F(NativeRendererMessagingServiceTest, DisconnectFromJS) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false, SerializationFormat::kJson);

  gin::Handle<GinPort> port = messaging_service()->CreatePortForTesting(
      script_context(), "channel", port_id);
  v8::Local<v8::Object> port_object = port.ToV8().As<v8::Object>();

  const char kDispatchMessage[] =
      "(function(port) {\n"
      "  port.disconnect();\n"
      "})";
  v8::Local<v8::Function> post_message =
      FunctionFromString(context, kDispatchMessage);
  v8::Local<v8::Value> args[] = {port_object};

  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  RunFunctionOnGlobal(post_message, context, std::size(args), args);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
}

TEST_F(NativeRendererMessagingServiceTest, Connect) {
  v8::HandleScope handle_scope(isolate());

  const std::string kChannel = "channel";
  PortId expected_port_id(script_context()->context_id(), 0, true,
                          SerializationFormat::kJson);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), expected_port_id, target,
                                     ChannelType::kConnect, kChannel));
  gin::Handle<GinPort> new_port = messaging_service()->Connect(
      script_context(), target, "channel", SerializationFormat::kJson);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ASSERT_FALSE(new_port.IsEmpty());

  EXPECT_EQ(expected_port_id, new_port->port_id());
  EXPECT_EQ(kChannel, new_port->name());
  EXPECT_FALSE(new_port->is_closed_for_testing());
}

// Tests sending a one-time message through the messaging service and getting a
// response to a callback. Note that this is more thoroughly tested in the
// OneTimeMessageHandler tests; this is just to ensure
// NativeRendererMessagingService correctly forwards the calls.
TEST_F(NativeRendererMessagingServiceTest, SendOneTimeMessageWithCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const ChannelType kChannel = ChannelType::kSendMessage;
  PortId port_id(script_context()->context_id(), 0, true,
                 SerializationFormat::kJson);
  const char kEchoArgs[] =
      "(function() { this.replyArgs = Array.from(arguments); })";
  v8::Local<v8::Function> response_callback =
      FunctionFromString(context, kEchoArgs);

  // Send a message and expect a reply to a passed in callback. A new port
  // should be created, and should remain open until the response is sent.
  const Message message("\"hi\"", SerializationFormat::kJson, false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     kChannel, "chrome.runtime.sendMessage"));
  EXPECT_CALL(*ipc_message_sender(), SendPostMessageToPort(port_id, message));
  v8::Local<v8::Promise> promise = messaging_service()->SendOneTimeMessage(
      script_context(), target, kChannel, message,
      binding::AsyncResponseType::kCallback, response_callback);
  // Since this is a callback based request, the returned promise should be
  // empty.
  EXPECT_TRUE(promise.IsEmpty());
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  // Respond to the message. The response callback should be triggered, and the
  // port should be closed.
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  messaging_service()->DeliverMessage(
      script_context_set(), port_id,
      Message("\"reply\"", SerializationFormat::kJson, false), nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_EQ("[\"reply\"]", GetStringPropertyFromObject(context->Global(),
                                                       context, "replyArgs"));
  EXPECT_FALSE(
      messaging_service()->HasPortForTesting(script_context(), port_id));
}

// Similar to the above test, tests sending a one-time message through the
// messaging service, but this time using a Promise for the response.
TEST_F(NativeRendererMessagingServiceTest, SendOneTimeMessageWithPromise) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const ChannelType kChannel = ChannelType::kSendMessage;
  PortId port_id(script_context()->context_id(), 0, true,
                 SerializationFormat::kJson);

  // Send a message and expect a reply fulfilling a promise. A new port should
  // be created, and should remain open until the response is sent.
  const Message message("\"hi\"", SerializationFormat::kJson, false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     kChannel, "chrome.runtime.sendMessage"));
  EXPECT_CALL(*ipc_message_sender(), SendPostMessageToPort(port_id, message));
  v8::Local<v8::Promise> promise = messaging_service()->SendOneTimeMessage(
      script_context(), target, kChannel, message,
      binding::AsyncResponseType::kPromise, v8::Local<v8::Function>());
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_EQ(v8::Promise::kPending, promise->State());
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  // Respond to the message. The response callback should be triggered, and the
  // port should be closed.
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  messaging_service()->DeliverMessage(
      script_context_set(), port_id,
      Message("\"reply\"", SerializationFormat::kJson, false), nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ("\"reply\"", V8ToString(promise->Result(), context));
  EXPECT_FALSE(
      messaging_service()->HasPortForTesting(script_context(), port_id));
}

// Tests receiving a one-time message through the messaging service. Note that
// this is more thoroughly tested in the OneTimeMessageHandler tests; this is
// just to ensure NativeRendererMessagingService correctly forwards the calls.
TEST_F(NativeRendererMessagingServiceTest, ReceiveOneTimeMessage) {
  RuntimeMessageValidationIgnorer message_validation_ignorer;

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "    this.eventMessage = message;\n"
      "    reply({data: 'hi'});\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  const std::string kChannel = "chrome.runtime.sendMessage";
  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false, SerializationFormat::kJson);

  ExtensionMsg_TabConnectionInfo tab_connection_info;
  tab_connection_info.frame_id = 0;
  const int tab_id = 10;
  GURL source_url("http://example.com");
  tab_connection_info.tab = DictionaryBuilder().Set("tabId", tab_id).Build();
  ExtensionMsg_ExternalConnectionInfo external_connection_info;
  external_connection_info.target_id = extension()->id();
  external_connection_info.source_endpoint =
      MessagingEndpoint::ForExtension(extension()->id());
  external_connection_info.source_url = source_url;
  external_connection_info.guest_process_id =
      content::kInvalidChildProcessUniqueId;
  external_connection_info.guest_render_frame_routing_id = 0;

  // Open a receiver for the message.
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessagePort(MSG_ROUTING_NONE, port_id));
  messaging_service()->DispatchOnConnect(
      script_context_set(), port_id, ChannelType::kSendMessage, kChannel,
      tab_connection_info, external_connection_info, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_TRUE(
      messaging_service()->HasPortForTesting(script_context(), port_id));

  // Post the message to the receiver. The receiver should respond, and the
  // port should close.
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(
                  port_id, Message(R"({"data":"hi"})",
                                   SerializationFormat::kJson, false)));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  messaging_service()->DeliverMessage(
      script_context_set(), port_id,
      Message("\"message\"", SerializationFormat::kJson, false), nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(
      messaging_service()->HasPortForTesting(script_context(), port_id));
}

// Test sending a one-time message from an external source (e.g., a different
// extension). This shouldn't conflict with messages sent from the same source.
TEST_F(NativeRendererMessagingServiceTest, TestExternalOneTimeMessages) {
  RuntimeMessageValidationIgnorer message_validation_ignorer;

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kListeners[] =
      R"((function() {
           chrome.runtime.onMessage.addListener((message) => {
             this.onMessageReceived = message;
             return true;  // Keep the channel open.
           });
           chrome.runtime.onMessageExternal.addListener((message) => {
             this.onMessageExternalReceived = message;
             return true;  // Keep the channel open.
           });
         }))";

  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kListeners);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  int next_port_id = 0;
  const PortId on_message_port_id(other_context_id, ++next_port_id, false,
                                  SerializationFormat::kJson);
  const PortId on_message_external_port_id(other_context_id, ++next_port_id,
                                           false, SerializationFormat::kJson);

  auto open_port = [this](const PortId& port_id, const ExtensionId& source_id) {
    ExtensionMsg_TabConnectionInfo tab_connection_info;
    tab_connection_info.frame_id = 0;
    const int tab_id = 10;
    GURL source_url("http://example.com");
    tab_connection_info.tab = DictionaryBuilder().Set("tabId", tab_id).Build();

    ExtensionMsg_ExternalConnectionInfo external_connection_info;
    external_connection_info.target_id = extension()->id();
    external_connection_info.source_endpoint =
        MessagingEndpoint::ForExtension(source_id);
    external_connection_info.source_url = source_url;
    external_connection_info.guest_process_id =
        content::kInvalidChildProcessUniqueId;
    external_connection_info.guest_render_frame_routing_id = 0;

    // Open a receiver for the message.
    EXPECT_CALL(*ipc_message_sender(),
                SendOpenMessagePort(MSG_ROUTING_NONE, port_id));
    messaging_service()->DispatchOnConnect(
        script_context_set(), port_id, ChannelType::kSendMessage,
        messaging_util::kSendMessageChannel, tab_connection_info,
        external_connection_info, nullptr);
    ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
    EXPECT_TRUE(
        messaging_service()->HasPortForTesting(script_context(), port_id));
  };

  open_port(on_message_port_id, extension()->id());
  const ExtensionId other_extension =
      crx_file::id_util::GenerateId("different");
  open_port(on_message_external_port_id, other_extension);

  EXPECT_CALL(*ipc_message_sender(),
              SendMessageResponsePending(MSG_ROUTING_NONE, on_message_port_id));
  messaging_service()->DeliverMessage(
      script_context_set(), on_message_port_id,
      Message("\"onMessage\"", SerializationFormat::kJson, false), nullptr);
  EXPECT_EQ("\"onMessage\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "onMessageReceived"));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(context->Global(), context,
                                        "onMessageExternalReceived"));

  messaging_service()->DeliverMessage(
      script_context_set(), on_message_external_port_id,
      Message("\"onMessageExternal\"", SerializationFormat::kJson, false),
      nullptr);
  EXPECT_EQ("\"onMessage\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "onMessageReceived"));
  EXPECT_EQ("\"onMessageExternal\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "onMessageExternalReceived"));
}

}  // namespace extensions
