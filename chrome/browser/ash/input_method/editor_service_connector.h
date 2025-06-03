// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SERVICE_CONNECTOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SERVICE_CONNECTOR_H_

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::input_method {
class EditorServiceConnector {
 public:
  EditorServiceConnector();
  ~EditorServiceConnector();

  void BindEditor(
      mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
          editor_client_connector,
      mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
          editor_event_sink,
      mojo::PendingAssociatedRemote<orca::mojom::TextActuator> text_actuator,
      mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
          text_query_provider);

  // returns true if the editor service requires a new editor service, and false
  // otherwise.
  bool SetUpNewEditorService();

  bool IsBound();

 private:
  mojo::Remote<orca::mojom::OrcaService> remote_orca_service_connector_;

  base::WeakPtrFactory<EditorServiceConnector> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SERVICE_CONNECTOR_H_
