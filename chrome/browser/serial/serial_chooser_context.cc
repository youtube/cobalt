// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/usb/usb_ids.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char kPortNameKey[] = "name";
constexpr char kTokenKey[] = "token";
#if BUILDFLAG(IS_WIN)
constexpr char kDeviceInstanceIdKey[] = "device_instance_id";
#else
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
constexpr char kSerialNumberKey[] = "serial_number";
#if BUILDFLAG(IS_MAC)
constexpr char kUsbDriverKey[] = "usb_driver";
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_WIN)

std::string EncodeToken(const base::UnguessableToken& token) {
  const uint64_t data[2] = {token.GetHighForSerialization(),
                            token.GetLowForSerialization()};
  std::string buffer;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), sizeof(data)),
      &buffer);
  return buffer;
}

base::UnguessableToken DecodeToken(base::StringPiece input) {
  std::string buffer;
  if (!base::Base64Decode(input, &buffer) ||
      buffer.length() != sizeof(uint64_t) * 2) {
    return base::UnguessableToken();
  }

  const uint64_t* data = reinterpret_cast<const uint64_t*>(buffer.data());
  absl::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(data[0], data[1]);
  if (!token.has_value()) {
    return base::UnguessableToken();
  }
  return token.value();
}

bool IsPolicyGrantedObject(const base::Value& object) {
  return object.is_dict() && object.GetDict().size() == 1 &&
         object.GetDict().contains(kPortNameKey);
}

base::Value VendorAndProductIdsToValue(uint16_t vendor_id,
                                       uint16_t product_id) {
  base::Value::Dict object;
  const char* product_name =
      device::UsbIds::GetProductName(vendor_id, product_id);
  if (product_name) {
    object.Set(kPortNameKey, product_name);
  } else {
    const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
    if (vendor_name) {
      object.Set(
          kPortNameKey,
          l10n_util::GetStringFUTF16(
              IDS_SERIAL_POLICY_DESCRIPTION_FOR_USB_PRODUCT_ID_AND_VENDOR_NAME,
              base::ASCIIToUTF16(base::StringPrintf("%04X", product_id)),
              base::UTF8ToUTF16(vendor_name)));
    } else {
      object.Set(
          kPortNameKey,
          l10n_util::GetStringFUTF16(
              IDS_SERIAL_POLICY_DESCRIPTION_FOR_USB_PRODUCT_ID_AND_VENDOR_ID,
              base::ASCIIToUTF16(base::StringPrintf("%04X", product_id)),
              base::ASCIIToUTF16(base::StringPrintf("%04X", vendor_id))));
    }
  }
  return base::Value(std::move(object));
}

base::Value VendorIdToValue(uint16_t vendor_id) {
  base::Value::Dict object;
  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    object.Set(kPortNameKey,
               l10n_util::GetStringFUTF16(
                   IDS_SERIAL_POLICY_DESCRIPTION_FOR_USB_VENDOR_NAME,
                   base::UTF8ToUTF16(vendor_name)));
  } else {
    object.Set(kPortNameKey,
               l10n_util::GetStringFUTF16(
                   IDS_SERIAL_POLICY_DESCRIPTION_FOR_USB_VENDOR_ID,
                   base::ASCIIToUTF16(base::StringPrintf("%04X", vendor_id))));
  }
  return base::Value(std::move(object));
}

void RecordPermissionRevocation(SerialPermissionRevoked type) {
  UMA_HISTOGRAM_ENUMERATION("Permissions.Serial.Revoked", type);
}

}  // namespace

SerialChooserContext::SerialChooserContext(Profile* profile)
    : ObjectPermissionContextBase(
          ContentSettingsType::SERIAL_GUARD,
          ContentSettingsType::SERIAL_CHOOSER_DATA,
          HostContentSettingsMapFactory::GetForProfile(profile)),
      profile_(profile) {
  DCHECK(profile_);
  permission_observation_.Observe(this);
}

SerialChooserContext::~SerialChooserContext() = default;

// static
base::Value SerialChooserContext::PortInfoToValue(
    const device::mojom::SerialPortInfo& port) {
  base::Value::Dict value;
  if (port.display_name && !port.display_name->empty()) {
    value.Set(kPortNameKey, *port.display_name);
  } else {
    value.Set(kPortNameKey, port.path.LossyDisplayName());
  }

  if (!SerialChooserContext::CanStorePersistentEntry(port)) {
    value.Set(kTokenKey, EncodeToken(port.token));
    return base::Value(std::move(value));
  }

#if BUILDFLAG(IS_WIN)
  // Windows provides a handy device identifier which we can rely on to be
  // sufficiently stable for identifying devices across restarts.
  value.Set(kDeviceInstanceIdKey, port.device_instance_id);
#else
  DCHECK(port.has_vendor_id);
  value.Set(kVendorIdKey, port.vendor_id);
  DCHECK(port.has_product_id);
  value.Set(kProductIdKey, port.product_id);
  DCHECK(port.serial_number);
  value.Set(kSerialNumberKey, *port.serial_number);

#if BUILDFLAG(IS_MAC)
  DCHECK(port.usb_driver_name && !port.usb_driver_name->empty());
  value.Set(kUsbDriverKey, *port.usb_driver_name);
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_WIN)
  return base::Value(std::move(value));
}

std::string SerialChooserContext::GetKeyForObject(const base::Value& object) {
  if (!IsValidObject(object))
    return std::string();

  if (IsPolicyGrantedObject(object)) {
    return *object.FindStringKey(kPortNameKey);
  }

#if BUILDFLAG(IS_WIN)
  return *(object.FindStringKey(kDeviceInstanceIdKey));
#else
  std::vector<std::string> key_pieces{
      base::NumberToString(*(object.FindIntKey(kVendorIdKey))),
      base::NumberToString(*(object.FindIntKey(kProductIdKey))),
      *(object.FindStringKey(kSerialNumberKey))};
#if BUILDFLAG(IS_MAC)
  key_pieces.push_back(*(object.FindStringKey(kUsbDriverKey)));
#endif  // BUILDFLAG(IS_MAC)
  return base::JoinString(key_pieces, "|");
#endif  // BUILDFLAG(IS_WIN)
}

bool SerialChooserContext::IsValidObject(const base::Value& object) {
  if (IsPolicyGrantedObject(object)) {
    return true;
  }

  if (!object.is_dict() || !object.FindStringKey(kPortNameKey))
    return false;

  const std::string* token = object.FindStringKey(kTokenKey);
  if (token)
    return object.DictSize() == 2 && DecodeToken(*token);

#if BUILDFLAG(IS_WIN)
  return object.DictSize() == 2 && object.FindStringKey(kDeviceInstanceIdKey);
#else
  if (!object.FindIntKey(kVendorIdKey) || !object.FindIntKey(kProductIdKey) ||
      !object.FindStringKey(kSerialNumberKey)) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  return object.DictSize() == 5 && object.FindStringKey(kUsbDriverKey);
#else
  return object.DictSize() == 4;
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_WIN)
}

std::u16string SerialChooserContext::GetObjectDisplayName(
    const base::Value& object) {
  const std::string* name = object.FindStringKey(kPortNameKey);
  DCHECK(name);
  return base::UTF8ToUTF16(*name);
}

void SerialChooserContext::OnPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : port_observer_list_)
    observer.OnPermissionRevoked(origin);
}

std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
SerialChooserContext::GetGrantedObjects(const url::Origin& origin) {
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetGrantedObjects(origin);

  if (CanRequestObjectPermission(origin)) {
    auto it = ephemeral_ports_.find(origin);
    if (it != ephemeral_ports_.end()) {
      const std::set<base::UnguessableToken>& ports = it->second;
      for (const auto& token : ports) {
        auto port_it = port_info_.find(token);
        if (port_it == port_info_.end())
          continue;

        const base::Value& port = PortInfoToValue(*port_it->second);
        objects.push_back(std::make_unique<Object>(
            origin, port.Clone(),
            content_settings::SettingSource::SETTING_SOURCE_USER,
            IsOffTheRecord()));
      }
    }
  }

  if (CanApplyPortSpecificPolicy()) {
    auto* policy = g_browser_process->serial_policy_allowed_ports();
    for (const auto& entry : policy->usb_device_policy()) {
      if (!base::Contains(entry.second, origin)) {
        continue;
      }

      base::Value object =
          VendorAndProductIdsToValue(entry.first.first, entry.first.second);
      objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
          origin, std::move(object), content_settings::SETTING_SOURCE_POLICY,
          IsOffTheRecord()));
    }

    for (const auto& entry : policy->usb_vendor_policy()) {
      if (!base::Contains(entry.second, origin)) {
        continue;
      }

      base::Value object = VendorIdToValue(entry.first);
      objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
          origin, std::move(object), content_settings::SETTING_SOURCE_POLICY,
          IsOffTheRecord()));
    }

    if (base::Contains(policy->all_ports_policy(), origin)) {
      base::Value::Dict object;
      object.Set(kPortNameKey, l10n_util::GetStringUTF16(
                                   IDS_SERIAL_POLICY_DESCRIPTION_FOR_ANY_PORT));
      objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
          origin, base::Value(std::move(object)),
          content_settings::SETTING_SOURCE_POLICY, IsOffTheRecord()));
    }
  }

  return objects;
}

std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
SerialChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetAllGrantedObjects();
  for (const auto& map_entry : ephemeral_ports_) {
    const url::Origin& origin = map_entry.first;

    if (!CanRequestObjectPermission(origin))
      continue;

    for (const auto& token : map_entry.second) {
      auto it = port_info_.find(token);
      if (it == port_info_.end())
        continue;

      objects.push_back(std::make_unique<Object>(
          origin, PortInfoToValue(*it->second),
          content_settings::SettingSource::SETTING_SOURCE_USER,
          IsOffTheRecord()));
    }
  }

  if (CanApplyPortSpecificPolicy()) {
    auto* policy = g_browser_process->serial_policy_allowed_ports();
    for (const auto& entry : policy->usb_device_policy()) {
      base::Value object =
          VendorAndProductIdsToValue(entry.first.first, entry.first.second);

      for (const auto& origin : entry.second) {
        objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
            origin, object.Clone(), content_settings::SETTING_SOURCE_POLICY,
            IsOffTheRecord()));
      }
    }

    for (const auto& entry : policy->usb_vendor_policy()) {
      base::Value object = VendorIdToValue(entry.first);

      for (const auto& origin : entry.second) {
        objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
            origin, object.Clone(), content_settings::SETTING_SOURCE_POLICY,
            IsOffTheRecord()));
      }
    }

    base::Value::Dict object;
    object.Set(kPortNameKey, l10n_util::GetStringUTF16(
                                 IDS_SERIAL_POLICY_DESCRIPTION_FOR_ANY_PORT));
    for (const auto& origin : policy->all_ports_policy()) {
      objects.push_back(std::make_unique<ObjectPermissionContextBase::Object>(
          origin, base::Value(object.Clone()),
          content_settings::SETTING_SOURCE_POLICY, IsOffTheRecord()));
    }
  }

  return objects;
}

void SerialChooserContext::RevokeObjectPermission(const url::Origin& origin,
                                                  const base::Value& object) {
  RevokeObjectPermissionInternal(origin, object, /*revoked_by_website=*/false);
}

void SerialChooserContext::RevokePortPermissionWebInitiated(
    const url::Origin& origin,
    const base::UnguessableToken& token) {
  auto it = port_info_.find(token);
  if (it == port_info_.end())
    return;

  return RevokeObjectPermissionInternal(origin, PortInfoToValue(*it->second),
                                        /*revoked_by_website=*/true);
}

void SerialChooserContext::RevokeObjectPermissionInternal(
    const url::Origin& origin,
    const base::Value& object,
    bool revoked_by_website = false) {
  const std::string* token = object.FindStringKey(kTokenKey);
  if (!token) {
    ObjectPermissionContextBase::RevokeObjectPermission(origin, object);
    RecordPermissionRevocation(
        revoked_by_website ? SerialPermissionRevoked::kPersistentByWebsite
                           : SerialPermissionRevoked::kPersistentByUser);
    return;
  }

  auto it = ephemeral_ports_.find(origin);
  if (it == ephemeral_ports_.end())
    return;
  std::set<base::UnguessableToken>& ports = it->second;

  DCHECK(IsValidObject(object));
  base::UnguessableToken decoded_token = DecodeToken(*token);
  if (decoded_token.is_empty()) {
    return;
  }
  ports.erase(std::move(decoded_token));
  RecordPermissionRevocation(revoked_by_website
                                 ? SerialPermissionRevoked::kEphemeralByWebsite
                                 : SerialPermissionRevoked::kEphemeralByUser);
  NotifyPermissionRevoked(origin);
}

void SerialChooserContext::GrantPortPermission(
    const url::Origin& origin,
    const device::mojom::SerialPortInfo& port) {
  port_info_.insert({port.token, port.Clone()});

  if (CanStorePersistentEntry(port)) {
    base::Value value = PortInfoToValue(port);
    GrantObjectPermission(origin, std::move(value));
    return;
  }

  ephemeral_ports_[origin].insert(port.token);
  NotifyPermissionChanged();
}

bool SerialChooserContext::HasPortPermission(
    const url::Origin& origin,
    const device::mojom::SerialPortInfo& port) {
  if (SerialBlocklist::Get().IsExcluded(port)) {
    return false;
  }

  if (CanApplyPortSpecificPolicy() &&
      g_browser_process->serial_policy_allowed_ports()->HasPortPermission(
          origin, port)) {
    return true;
  }

  if (!CanRequestObjectPermission(origin)) {
    return false;
  }

  auto it = ephemeral_ports_.find(origin);
  if (it != ephemeral_ports_.end()) {
    const std::set<base::UnguessableToken> ports = it->second;
    if (base::Contains(ports, port.token))
      return true;
  }

  if (!CanStorePersistentEntry(port)) {
    return false;
  }

  std::vector<std::unique_ptr<Object>> object_list =
      ObjectPermissionContextBase::GetGrantedObjects(origin);
  for (const auto& object : object_list) {
    const base::Value& device = object->value;

    // Objects provided by the parent class can be assumed valid.
    DCHECK(IsValidObject(device));

#if BUILDFLAG(IS_WIN)
    const std::string& device_instance_id =
        *device.FindStringKey(kDeviceInstanceIdKey);
    if (port.device_instance_id == device_instance_id)
      return true;
#else
    const int vendor_id = *device.FindIntKey(kVendorIdKey);
    const int product_id = *device.FindIntKey(kProductIdKey);
    const std::string& serial_number = *device.FindStringKey(kSerialNumberKey);

    // Guaranteed by the CanStorePersistentEntry() check above.
    DCHECK(port.has_vendor_id);
    DCHECK(port.has_product_id);
    DCHECK(port.serial_number && !port.serial_number->empty());
    if (port.vendor_id != vendor_id || port.product_id != product_id ||
        port.serial_number != serial_number) {
      continue;
    }

#if BUILDFLAG(IS_MAC)
    const std::string& usb_driver_name = *device.FindStringKey(kUsbDriverKey);
    if (port.usb_driver_name != usb_driver_name) {
      continue;
    }
#endif  // BUILDFLAG(IS_MAC)

    return true;
#endif  // BUILDFLAG(IS_WIN)
  }
  return false;
}

// static
bool SerialChooserContext::CanStorePersistentEntry(
    const device::mojom::SerialPortInfo& port) {
  // If there is no display name then the path name will be used instead. The
  // path name is not guaranteed to be stable. For example, on Linux the name
  // "ttyUSB0" is reused for any USB serial device. A name like that would be
  // confusing to show in settings when the device is disconnected.
  if (!port.display_name || port.display_name->empty())
    return false;

#if BUILDFLAG(IS_WIN)
  return !port.device_instance_id.empty();
#else
  if (!port.has_vendor_id || !port.has_product_id || !port.serial_number ||
      port.serial_number->empty()) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  // The combination of the standard USB vendor ID, product ID and serial
  // number properties should be enough to uniquely identify a device
  // however recent versions of macOS include built-in drivers for common
  // types of USB-to-serial adapters while their manufacturers still
  // recommend installing their custom drivers. When both are loaded two
  // IOSerialBSDClient instances are found for each device. Including the
  // USB driver name allows us to distinguish between the two.
  if (!port.usb_driver_name || port.usb_driver_name->empty())
    return false;
#endif  // BUILDFLAG(IS_MAC)

  return true;
#endif  // BUILDFLAG(IS_WIN)
}

const device::mojom::SerialPortInfo* SerialChooserContext::GetPortInfo(
    const base::UnguessableToken& token) {
  DCHECK(is_initialized_);
  auto it = port_info_.find(token);
  return it == port_info_.end() ? nullptr : it->second.get();
}

device::mojom::SerialPortManager* SerialChooserContext::GetPortManager() {
  EnsurePortManagerConnection();
  return port_manager_.get();
}

void SerialChooserContext::AddPortObserver(PortObserver* observer) {
  port_observer_list_.AddObserver(observer);
}

void SerialChooserContext::RemovePortObserver(PortObserver* observer) {
  port_observer_list_.RemoveObserver(observer);
}

void SerialChooserContext::SetPortManagerForTesting(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::FlushPortManagerConnectionForTesting() {
  port_manager_.FlushForTesting();
}

base::WeakPtr<SerialChooserContext> SerialChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SerialChooserContext::OnPortAdded(device::mojom::SerialPortInfoPtr port) {
  if (!base::Contains(port_info_, port->token)) {
    port_info_.insert({port->token, port->Clone()});
  }

  for (auto& observer : port_observer_list_)
    observer.OnPortAdded(*port);
}

void SerialChooserContext::OnPortRemoved(
    device::mojom::SerialPortInfoPtr port) {
  for (auto& observer : port_observer_list_)
    observer.OnPortRemoved(*port);

  std::vector<url::Origin> revoked_origins;
  for (auto& map_entry : ephemeral_ports_) {
    std::set<base::UnguessableToken>& ports = map_entry.second;
    if (ports.erase(port->token) > 0) {
      RecordPermissionRevocation(
          SerialPermissionRevoked::kEphemeralByDisconnect);
      revoked_origins.push_back(map_entry.first);
    }
  }

  port_info_.erase(port->token);

  for (auto& observer : permission_observer_list_) {
    if (!revoked_origins.empty()) {
      observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                         data_content_settings_type_);
    }
    for (const auto& origin : revoked_origins)
      observer.OnPermissionRevoked(origin);
  }
}

void SerialChooserContext::EnsurePortManagerConnection() {
  if (port_manager_)
    return;

  mojo::PendingRemote<device::mojom::SerialPortManager> manager;
  content::GetDeviceService().BindSerialPortManager(
      manager.InitWithNewPipeAndPassReceiver());
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::SetUpPortManagerConnection(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  port_manager_.Bind(std::move(manager));
  port_manager_.set_disconnect_handler(
      base::BindOnce(&SerialChooserContext::OnPortManagerConnectionError,
                     base::Unretained(this)));

  port_manager_->SetClient(client_receiver_.BindNewPipeAndPassRemote());
  port_manager_->GetDevices(base::BindOnce(&SerialChooserContext::OnGetDevices,
                                           weak_factory_.GetWeakPtr()));
}

void SerialChooserContext::OnGetDevices(
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  for (auto& port : ports)
    port_info_.insert({port->token, std::move(port)});
  is_initialized_ = true;
}

void SerialChooserContext::OnPortManagerConnectionError() {
  port_manager_.reset();
  client_receiver_.reset();

  port_info_.clear();

  std::vector<url::Origin> revoked_origins;
  revoked_origins.reserve(ephemeral_ports_.size());
  for (const auto& map_entry : ephemeral_ports_)
    revoked_origins.push_back(map_entry.first);
  ephemeral_ports_.clear();

  // Notify permission observers that all ephemeral permissions have been
  // revoked.
  for (auto& observer : permission_observer_list_) {
    observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                       data_content_settings_type_);
    for (const auto& origin : revoked_origins)
      observer.OnPermissionRevoked(origin);
  }
}

bool SerialChooserContext::CanApplyPortSpecificPolicy() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* profile_helper = ash::ProfileHelper::Get();
  DCHECK(profile_helper);
  user_manager::User* user = profile_helper->GetUserByProfile(profile_);
  DCHECK(user);
  return user->IsAffiliated();
#else
  return true;
#endif
}
