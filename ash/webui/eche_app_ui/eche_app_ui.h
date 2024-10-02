// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-forward.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::eche_app {

// The WebUI for chrome://eche-app/.
class EcheAppUI : public ui::MojoWebUIController {
 public:
  using BindSignalingMessageExchangerCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger>)>;
  using BindSystemInfoProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::SystemInfoProvider>)>;
  using BindAccessibilityProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::AccessibilityProvider>)>;
  using BindUidGeneratorCallback =
      base::RepeatingCallback<void(mojo::PendingReceiver<mojom::UidGenerator>)>;
  using BindNotificationGeneratorCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::NotificationGenerator>)>;
  using BindDisplayStreamHandlerCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::DisplayStreamHandler>)>;
  using BindStreamOrientationObserverCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::StreamOrientationObserver>)>;
  using BindConnectionStatusObserverCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::ConnectionStatusObserver>)>;

  EcheAppUI(
      content::WebUI* web_ui,
      BindSignalingMessageExchangerCallback exchanger_callback,
      BindSystemInfoProviderCallback system_info_callback,
      BindAccessibilityProviderCallback accessibility_callback,
      BindUidGeneratorCallback generator_callback,
      BindNotificationGeneratorCallback notification_callback,
      BindDisplayStreamHandlerCallback stream_handler_callback,
      BindStreamOrientationObserverCallback stream_orientation_callback,
      BindConnectionStatusObserverCallback connection_status_changed_callback);
  EcheAppUI(const EcheAppUI&) = delete;
  EcheAppUI& operator=(const EcheAppUI&) = delete;
  ~EcheAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  void BindInterface(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::AccessibilityProvider> receiver);

  void BindInterface(mojo::PendingReceiver<mojom::UidGenerator> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver);

 private:
  const BindSignalingMessageExchangerCallback bind_exchanger_callback_;
  const BindSystemInfoProviderCallback bind_system_info_callback_;
  const BindAccessibilityProviderCallback bind_accessibility_callback;
  const BindUidGeneratorCallback bind_generator_callback_;
  const BindNotificationGeneratorCallback bind_notification_callback_;
  const BindDisplayStreamHandlerCallback bind_stream_handler_callback_;
  const BindStreamOrientationObserverCallback bind_stream_orientation_callback_;
  const BindConnectionStatusObserverCallback
      bind_connection_status_changed_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_
