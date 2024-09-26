// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_
#define ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_

#include <memory>
#include <string>

namespace ash {
namespace eche_app {

// Stores system information for Eche app.
class SystemInfo {
 public:
  class Builder {
   public:
    Builder();
    virtual ~Builder();

    std::unique_ptr<SystemInfo> Build();
    Builder& SetBoardName(const std::string& board_name);
    Builder& SetDeviceName(const std::string& device_name);
    Builder& SetGaiaId(const std::string& gaia_id);
    Builder& SetDeviceType(const std::string& device_type);

   private:
    std::string board_name_;
    std::string device_name_;
    std::string gaia_id_;
    std::string device_type_;
  };

  SystemInfo(const SystemInfo& other);
  virtual ~SystemInfo();

  std::string GetDeviceName() const { return device_name_; }
  std::string GetBoardName() const { return board_name_; }
  std::string GetGaiaId() const { return gaia_id_; }
  std::string GetDeviceType() const { return device_type_; }

 protected:
  SystemInfo(const std::string& device_name,
             const std::string& board_name,
             const std::string& gaia_id,
             const std::string& device_type);

 private:
  std::string device_name_;
  std::string board_name_;
  std::string gaia_id_;
  std::string device_type_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_
