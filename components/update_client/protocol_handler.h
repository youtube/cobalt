// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_HANDLER_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_HANDLER_H_

#include <memory>

namespace update_client {

class ProtocolParser;
class ProtocolSerializer;

class ProtocolHandlerFactory {
 public:
  ProtocolHandlerFactory(const ProtocolHandlerFactory&) = delete;
  ProtocolHandlerFactory& operator=(const ProtocolHandlerFactory&) = delete;

  virtual ~ProtocolHandlerFactory() = default;
  virtual std::unique_ptr<ProtocolParser> CreateParser() const = 0;
  virtual std::unique_ptr<ProtocolSerializer> CreateSerializer() const = 0;

 protected:
  ProtocolHandlerFactory() = default;
};

class ProtocolHandlerFactoryJSON final : public ProtocolHandlerFactory {
 public:
  // Overrides for ProtocolHandlerFactory.
  std::unique_ptr<ProtocolParser> CreateParser() const override;
  std::unique_ptr<ProtocolSerializer> CreateSerializer() const override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_HANDLER_H_
