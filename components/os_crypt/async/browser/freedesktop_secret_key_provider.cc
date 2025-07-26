// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"

#include <algorithm>
#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "crypto/encryptor.h"
#include "crypto/kdf.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace os_crypt_async {

namespace {

constexpr char kUmaInitStatus[] =
    "OSCrypt.FreedesktopSecretKeyProvider.InitStatus";
constexpr char kUmaErrorDetail[] =
    "OSCrypt.FreedesktopSecretKeyProvider.$1.ErrorDetail";

// These constants are duplicated from the sync backend.
constexpr char kEncryptionTag[] = "v11";
constexpr char kSalt[] = "saltysalt";
constexpr size_t kDerivedKeySizeInBits = 128;
constexpr size_t kEncryptionIterations = 1;
constexpr size_t kSecretLengthBytes = 16;

template <typename ReplyArgs>
void CallMethod(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const std::string& method_name,
    const DbusType& arguments,
    base::OnceCallback<void(
        base::expected<ReplyArgs, FreedesktopSecretKeyProvider::ErrorDetail>)>
        callback) {
  dbus::MethodCall method_call(interface_name, method_name);
  dbus::MessageWriter writer(&method_call);
  arguments.Write(&writer);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          [](const std::string& interface_name, const std::string& method_name,
             base::OnceCallback<void(
                 base::expected<ReplyArgs,
                                FreedesktopSecretKeyProvider::ErrorDetail>)>
                 callback,
             dbus::Response* response) {
            using ErrorDetail = FreedesktopSecretKeyProvider::ErrorDetail;
            if (!response) {
              std::move(callback).Run(
                  base::unexpected(ErrorDetail::kNoResponse));
              return;
            }
            dbus::MessageReader reader(response);
            ReplyArgs reply;
            if (!reply.Read(&reader)) {
              LOG(ERROR) << "Failed to read reply for " << interface_name << "."
                         << method_name << ": expected type "
                         << ReplyArgs::GetSignature() << " but got type "
                         << response->GetSignature();
              std::move(callback).Run(
                  base::unexpected(ErrorDetail::kInvalidReplyFormat));
              return;
            }
            std::move(callback).Run(std::move(reply));
          },
          interface_name, method_name, std::move(callback)));
}

const char* InitStatusToString(
    FreedesktopSecretKeyProvider::InitStatus status) {
  switch (status) {
    case FreedesktopSecretKeyProvider::InitStatus::kSuccess:
      return "Success";
    case FreedesktopSecretKeyProvider::InitStatus::kCreateCollectionFailed:
      return "CreateCollectionFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kCreateItemFailed:
      return "CreateItemFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kEmptySecret:
      return "EmptySecret";
    case FreedesktopSecretKeyProvider::InitStatus::kGetSecretFailed:
      return "GetSecretFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kGnomeKeyringDeadlock:
      return "GnomeKeyringDeadlock";
    case FreedesktopSecretKeyProvider::InitStatus::kNoService:
      return "NoService";
    case FreedesktopSecretKeyProvider::InitStatus::kReadAliasFailed:
      return "ReadAliasFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kSearchItemsFailed:
      return "SearchItemsFailed";
    case FreedesktopSecretKeyProvider::InitStatus::kSessionFailure:
      return "SessionFailure";
    case FreedesktopSecretKeyProvider::InitStatus::kUnlockFailed:
      return "UnlockFailed";
  }
  NOTREACHED();
}

}  // namespace

// A helper class to handle a Secret Service prompt. It is templated on the
// return type expected from the prompt.
template <typename T>
class FreedesktopSecretKeyProvider::Prompter
    : public base::RefCountedThreadSafe<Prompter<T>> {
 public:
  using PromptCallback = base::OnceCallback<void(
      base::expected<T, FreedesktopSecretKeyProvider::ErrorDetail>)>;

  static void Prompt(scoped_refptr<dbus::Bus> bus,
                     dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const std::string& method_name,
                     const DbusType& arguments,
                     PromptCallback callback) {
    auto handler =
        base::MakeRefCounted<Prompter<T>>(std::move(bus), std::move(callback));
    CallMethod(object_proxy, interface_name, method_name, arguments,
               base::BindOnce(&Prompter::OnReply, handler));
  }

  Prompter(scoped_refptr<dbus::Bus> bus, PromptCallback callback)
      : bus_(std::move(bus)), callback_(std::move(callback)) {}

  Prompter(const Prompter&) = delete;
  Prompter& operator=(const Prompter&) = delete;

 private:
  friend class base::RefCountedThreadSafe<Prompter<T>>;
  using ErrorDetail = FreedesktopSecretKeyProvider::ErrorDetail;

  ~Prompter() {
    Finish(base::unexpected(ErrorDetail::kDestructedBeforeComplete));
  }

  void OnReply(
      base::expected<DbusParameters<T, DbusObjectPath>, ErrorDetail> reply) {
    if (!reply.has_value()) {
      Finish(base::unexpected(reply.error()));
      return;
    }
    auto& [value, prompt] = reply->value();
    if (prompt.value().value() == "/") {
      Finish(std::move(value));
    } else {
      prompt_path_ = prompt.value();
      StartPrompt();
    }
  }

  void StartPrompt() {
    auto* prompt_proxy = bus_->GetObjectProxy(
        FreedesktopSecretKeyProvider::kSecretServiceName, prompt_path_);
    prompt_proxy->ConnectToSignal(
        FreedesktopSecretKeyProvider::kSecretPromptInterface, "Completed",
        base::BindRepeating(&Prompter::OnPromptCompletedSignal, this),
        base::BindOnce(&Prompter::OnSignalConnected, this));
    CallMethod(prompt_proxy,
               FreedesktopSecretKeyProvider::kSecretPromptInterface,
               FreedesktopSecretKeyProvider::kMethodPrompt, DbusString(""),
               base::BindOnce(&Prompter::OnPromptResponse, this));
  }

  void OnPromptResponse(base::expected<DbusVoid, ErrorDetail> response) {
    if (!response.has_value()) {
      LOG(ERROR) << "Prompt call returned no response.";
      Finish(base::unexpected(response.error()));
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected) {
    if (!connected) {
      LOG(ERROR) << "Failed to connect to Prompt.Completed signal.";
      Finish(base::unexpected(ErrorDetail::kPromptFailedSignalConnection));
    }
  }

  void OnPromptCompletedSignal(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    DbusParameters<DbusBoolean, DbusVariant> args;
    if (!args.Read(&reader)) {
      LOG(ERROR) << "Failed to read Prompt.Completed signal args.";
      Finish(base::unexpected(ErrorDetail::kInvalidSignalFormat));
      return;
    }

    auto& [dismissed, variant] = args.value();
    if (dismissed.value()) {
      Finish(base::unexpected(ErrorDetail::kPromptDismissed));
      return;
    }

    T* value = variant.GetAs<T>();
    if (!value) {
      LOG(ERROR) << "Failed to parse prompt result.";
      Finish(base::unexpected(ErrorDetail::kInvalidVariantFormat));
      return;
    }

    Finish(base::ok(std::move(*value)));
  }

  void Finish(base::expected<T, ErrorDetail> result) {
    if (!prompt_path_.value().empty()) {
      bus_->RemoveObjectProxy(FreedesktopSecretKeyProvider::kSecretServiceName,
                              prompt_path_, base::DoNothing());
      prompt_path_ = dbus::ObjectPath();
    }
    if (callback_) {
      std::move(callback_).Run(std::move(result));
    }
    bus_.reset();
  }

  scoped_refptr<dbus::Bus> bus_;
  PromptCallback callback_;
  dbus::ObjectPath prompt_path_;
};

FreedesktopSecretKeyProvider::FreedesktopSecretKeyProvider(
    bool use_for_encryption,
    const std::string& product_name,
    scoped_refptr<dbus::Bus> bus)
    : use_for_encryption_(use_for_encryption),
      product_name_(product_name),
      bus_(std::move(bus)) {
  if (!bus_) {
    bus_ = dbus_thread_linux::GetSharedSessionBus();
  }
}

FreedesktopSecretKeyProvider::~FreedesktopSecretKeyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FreedesktopSecretKeyProvider::GetKey(KeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  key_callback_ = std::move(callback);

  if (!secret_for_testing_.empty()) {
    DeriveKeyFromSecret(base::as_byte_span(secret_for_testing_));
    return;
  }

  dbus_utils::CheckForServiceAndStart(
      bus_, kSecretServiceName,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool FreedesktopSecretKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

bool FreedesktopSecretKeyProvider::IsCompatibleWithOsCryptSync() {
  return true;
}

void FreedesktopSecretKeyProvider::OnServiceStarted(
    std::optional<bool> service_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_started.value_or(false)) {
    FinalizeFailure(InitStatus::kNoService, ErrorDetail::kNone);
    return;
  }

  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  CallMethod(service_proxy, kSecretServiceInterface, kMethodReadAlias,
             DbusString(kDefaultAlias),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnReadAliasDefault,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnReadAliasDefault(
    base::expected<DbusObjectPath, ErrorDetail> collection_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!collection_path.has_value()) {
    FinalizeFailure(InitStatus::kReadAliasFailed, collection_path.error());
    return;
  }
  if (collection_path->value().value() != "/") {
    default_collection_proxy_ =
        bus_->GetObjectProxy(kSecretServiceName, collection_path->value());
    CallMethod(default_collection_proxy_, kPropertiesInterface, kMethodGet,
               MakeDbusParameters(DbusString(kSecretCollectionInterface),
                                  DbusString(kLabelProperty)),
               base::BindOnce(
                   &FreedesktopSecretKeyProvider::OnGetCollectionLabelResponse,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    // No default collection, create it
    auto* service_proxy = bus_->GetObjectProxy(
        kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
    DbusDictionary props = MakeDbusDictionary(
        kSecretCollectionLabelProperty, DbusString(kDefaultCollectionLabel));

    Prompter<DbusObjectPath>::Prompt(
        bus_, service_proxy, kSecretServiceInterface, kMethodCreateCollection,
        MakeDbusParameters(std::move(props), DbusString(kDefaultAlias)),
        base::BindOnce(&FreedesktopSecretKeyProvider::OnCreateCollection,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FreedesktopSecretKeyProvider::OnGetCollectionLabelResponse(
    base::expected<DbusVariant, ErrorDetail> variant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!variant.has_value()) {
    LOG(ERROR) << "Get(Label) failed.";
    FinalizeFailure(InitStatus::kGnomeKeyringDeadlock, variant.error());
    return;
  }

  const DbusString* label_variant = variant->GetAs<DbusString>();
  if (!label_variant) {
    LOG(ERROR) << "Label property missing or invalid.";
    FinalizeFailure(InitStatus::kGnomeKeyringDeadlock,
                    ErrorDetail::kInvalidVariantFormat);
    return;
  }

  // Label property read successfully
  UnlockDefaultCollection();
}

void FreedesktopSecretKeyProvider::OnCreateCollection(
    base::expected<DbusObjectPath, ErrorDetail> create_collection_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_collection_reply.has_value()) {
    FinalizeFailure(InitStatus::kCreateCollectionFailed,
                    create_collection_reply.error());
    return;
  }
  if (create_collection_reply->value().value() == "/") {
    FinalizeFailure(InitStatus::kCreateCollectionFailed,
                    ErrorDetail::kEmptyObjectPaths);
    return;
  }
  default_collection_proxy_ = bus_->GetObjectProxy(
      kSecretServiceName, create_collection_reply->value());
  UnlockDefaultCollection();
}

void FreedesktopSecretKeyProvider::UnlockDefaultCollection() {
  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));

  auto objects =
      MakeDbusArray(DbusObjectPath(default_collection_proxy_->object_path()));
  Prompter<DbusArray<DbusObjectPath>>::Prompt(
      bus_, service_proxy, kSecretServiceInterface, kMethodUnlock, objects,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnUnlock,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnUnlock(
    base::expected<DbusArray<DbusObjectPath>, ErrorDetail>
        unlocked_collection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!unlocked_collection.has_value()) {
    FinalizeFailure(InitStatus::kUnlockFailed, unlocked_collection.error());
    return;
  }
  if (unlocked_collection->value().empty()) {
    FinalizeFailure(InitStatus::kUnlockFailed, ErrorDetail::kEmptyObjectPaths);
    return;
  }
  // Unlocked now
  OpenSession();
}

void FreedesktopSecretKeyProvider::OpenSession() {
  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  CallMethod(service_proxy, kSecretServiceInterface, kMethodOpenSession,
             MakeDbusParameters(DbusString(kAlgorithmPlain),
                                MakeDbusVariant(DbusString(kInputPlain))),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnOpenSession,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnOpenSession(
    base::expected<DbusParameters<DbusVariant, DbusObjectPath>, ErrorDetail>
        session_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_reply.has_value()) {
    FinalizeFailure(InitStatus::kSessionFailure, session_reply.error());
    return;
  }
  const auto& [_, result] = session_reply->value();
  session_proxy_ = bus_->GetObjectProxy(kSecretServiceName, result.value());
  session_opened_ = true;

  auto search_attrs = MakeDbusArray(MakeDbusDictEntry(
      DbusString(kApplicationAttributeKey), DbusString(kAppName)));

  CallMethod(default_collection_proxy_, kSecretCollectionInterface,
             kMethodSearchItems, search_attrs,
             base::BindOnce(&FreedesktopSecretKeyProvider::OnSearchItems,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnSearchItems(
    base::expected<DbusArray<DbusObjectPath>, ErrorDetail> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!results.has_value()) {
    FinalizeFailure(InitStatus::kSearchItemsFailed, results.error());
    return;
  }

  if (results->value().empty()) {
    // No items found, attempt KWallet migration.
    TryKWalletMigration();
    return;
  }

  auto* item_proxy = bus_->GetObjectProxy(kSecretServiceName,
                                          results->value().front().value());
  CallMethod(item_proxy, kSecretItemInterface, kMethodGetSecret,
             MakeDbusParameters(DbusObjectPath(session_proxy_->object_path())),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnGetSecret,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnGetSecret(
    base::expected<DbusSecret, ErrorDetail> secret_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!secret_reply.has_value()) {
    FinalizeFailure(InitStatus::kGetSecretFailed, secret_reply.error());
    return;
  }

  const auto& [session_path, parameters, value, content_type] =
      secret_reply->value();
  const auto& secret_bytes = value.value();
  if (!secret_bytes) {
    FinalizeFailure(InitStatus::kGetSecretFailed,
                    ErrorDetail::kInvalidVariantFormat);
    return;
  }
  if (secret_bytes->size() == 0) {
    LOG(ERROR) << "GetSecret returned an empty secret.";
    FinalizeFailure(InitStatus::kEmptySecret, ErrorDetail::kNone);
    return;
  }

  DeriveKeyFromSecret(base::span(*secret_bytes));
}

void FreedesktopSecretKeyProvider::TryKWalletMigration() {
  if (kwallet_candidate_index_ >= kKWalletCandidates.size()) {
    // No KWallet
    CreateItem(base::MakeRefCounted<base::RefCountedString>(
        base::Base64Encode(base::RandBytesAsVector(kSecretLengthBytes))));
    return;
  }

  const auto& service_and_path = kKWalletCandidates[kwallet_candidate_index_];
  kwallet_proxy_ =
      bus_->GetObjectProxy(service_and_path.kwallet_service,
                           dbus::ObjectPath(service_and_path.kwallet_path));
  dbus_utils::NameHasOwner(
      bus_.get(), service_and_path.kwallet_service,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnNameHasOwnerForKWallet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnNameHasOwnerForKWallet(
    std::optional<bool> has_owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!has_owner.value_or(false)) {
    kwallet_candidate_index_++;
    TryKWalletMigration();
    return;
  }

  CallMethod(kwallet_proxy_, kKWalletInterface, kKWalletMethodIsEnabled,
             DbusVoid(),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletIsEnabled,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletIsEnabled(
    base::expected<DbusBoolean, ErrorDetail> is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled.has_value() || !is_enabled->value()) {
    kwallet_candidate_index_++;
    TryKWalletMigration();
    return;
  }
  CallMethod(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodNetworkWallet,
      DbusVoid(),
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletNetworkWallet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletNetworkWallet(
    base::expected<DbusString, ErrorDetail> wallet_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!wallet_name.has_value()) {
    kwallet_candidate_index_++;
    TryKWalletMigration();
    return;
  }
  CallMethod(kwallet_proxy_, kKWalletInterface, kKWalletMethodOpen,
             MakeDbusParameters(std::move(*wallet_name), DbusInt64(0),
                                DbusString(product_name_)),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletOpen,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnKWalletOpen(
    base::expected<DbusInt32, ErrorDetail> handle_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int32_t handle = handle_reply.has_value() ? handle_reply->value() : -1;
  if (handle < 0) {
    kwallet_candidate_index_++;
    TryKWalletMigration();
    return;
  }
  CallMethod(
      kwallet_proxy_, kKWalletInterface, kKWalletMethodReadPassword,
      MakeDbusParameters(DbusInt32(handle), DbusString(kKWalletFolder),
                         DbusString(kKeyName), DbusString(product_name_)),
      base::BindOnce(&FreedesktopSecretKeyProvider::OnKWalletReadPassword,
                     weak_ptr_factory_.GetWeakPtr(), handle));
}

void FreedesktopSecretKeyProvider::OnKWalletReadPassword(
    int32_t handle,
    base::expected<DbusString, ErrorDetail> secret_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CallMethod(kwallet_proxy_, kKWalletInterface, kKWalletMethodClose,
             MakeDbusParameters(DbusInt32(handle), DbusBoolean(false),
                                DbusString(product_name_)),
             base::BindOnce([](base::expected<DbusInt32, ErrorDetail>) {}));

  if (!secret_reply.has_value() || secret_reply->value().empty()) {
    // No secret found. Try next candidate.
    kwallet_candidate_index_++;
    TryKWalletMigration();
    return;
  }

  CreateItem(
      base::MakeRefCounted<base::RefCountedString>(secret_reply->value()));
}

void FreedesktopSecretKeyProvider::CreateItem(
    scoped_refptr<base::RefCountedMemory> secret) {
  auto attributes =
      MakeDbusArray(MakeDbusDictEntry(DbusString(kApplicationAttributeKey),
                                      DbusString(kAppName)),
                    MakeDbusDictEntry(DbusString(kSchemaAttributeKey),
                                      DbusString(kSchemaAttributeValue)));
  auto props =
      MakeDbusDictionary(kSecretItemAttributesProperty, std::move(attributes),
                         kSecretItemLabelProperty, DbusString(kKeyName));

  auto secret_struct = MakeDbusStruct(
      DbusObjectPath(session_proxy_->object_path()),
      DbusByteArray(base::MakeRefCounted<base::RefCountedBytes>()),
      DbusByteArray(secret), DbusString(kMimePlain));
  auto* collection_proxy = bus_->GetObjectProxy(
      kSecretServiceName, default_collection_proxy_->object_path());
  Prompter<DbusObjectPath>::Prompt(
      bus_, collection_proxy, kSecretCollectionInterface, kMethodCreateItem,
      MakeDbusParameters(std::move(props), std::move(secret_struct),
                         DbusBoolean(false)),
      base::BindOnce(&FreedesktopSecretKeyProvider::OnCreateItem,
                     weak_ptr_factory_.GetWeakPtr(), std::move(secret)));
}

void FreedesktopSecretKeyProvider::OnCreateItem(
    scoped_refptr<base::RefCountedMemory> secret,
    base::expected<DbusObjectPath, ErrorDetail> created_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_item.has_value()) {
    FinalizeFailure(InitStatus::kCreateItemFailed, created_item.error());
    return;
  }
  if (created_item->value().value().empty()) {
    FinalizeFailure(InitStatus::kCreateItemFailed,
                    ErrorDetail::kEmptyObjectPaths);
    return;
  }
  DeriveKeyFromSecret(*secret);
}

void FreedesktopSecretKeyProvider::DeriveKeyFromSecret(
    base::span<const uint8_t> secret) {
  static_assert(kDerivedKeySizeInBits % 8 == 0);
  std::array<uint8_t, kDerivedKeySizeInBits / 8> key_bytes;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      {kEncryptionIterations}, secret,
      base::as_byte_span(base::span_from_cstring(kSalt)), key_bytes,
      crypto::SubtlePassKey{});
  Encryptor::Key key(key_bytes, mojom::Algorithm::kAES128CBC);
  FinalizeSuccess(std::move(key));
}

void FreedesktopSecretKeyProvider::FinalizeSuccess(Encryptor::Key key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordInitStatus(InitStatus::kSuccess, ErrorDetail::kNone);
  std::move(key_callback_).Run(kEncryptionTag, std::move(key));
  CloseSession();
}

void FreedesktopSecretKeyProvider::FinalizeFailure(InitStatus status,
                                                   ErrorDetail detail) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!key_callback_) {
    return;
  }
  RecordInitStatus(status, detail);
  // TODO(crbug.com/389016528): Indicate whether this is a temporary or
  // permanent failure depending on the `status` and `detail`.
  std::move(key_callback_)
      .Run(kEncryptionTag, base::unexpected(KeyError::kTemporarilyUnavailable));
  CloseSession();
}

void FreedesktopSecretKeyProvider::RecordInitStatus(InitStatus status,
                                                    ErrorDetail detail) {
  // Log the high-level InitStatus.
  base::UmaHistogramEnumeration(kUmaInitStatus, status);

  // If there was an error, also log the error detail.
  if (status != InitStatus::kSuccess) {
    auto histogram_name = base::ReplaceStringPlaceholders(
        kUmaErrorDetail, std::vector<std::string>{InitStatusToString(status)},
        nullptr);
    base::UmaHistogramEnumeration(histogram_name, detail);
  }
}

void FreedesktopSecretKeyProvider::CloseSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_opened_) {
    CallMethod(session_proxy_, kSecretSessionInterface, kMethodClose,
               DbusVoid(),
               base::BindOnce([](base::expected<DbusVoid, ErrorDetail>) {}));
  }
}

}  // namespace os_crypt_async
