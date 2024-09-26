// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/key_storage_keyring.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/sync/keyring_util_linux.h"

namespace {

const GnomeKeyringPasswordSchema kSchema = {
    GNOME_KEYRING_ITEM_GENERIC_SECRET,
    {{"application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING}, {nullptr}}};

}  // namespace

KeyStorageKeyring::KeyStorageKeyring(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    std::string application_name)
    : main_thread_runner_(main_thread_runner),
      application_name_(std::move(application_name)) {}

KeyStorageKeyring::~KeyStorageKeyring() {}

base::SequencedTaskRunner* KeyStorageKeyring::GetTaskRunner() {
  return main_thread_runner_.get();
}

bool KeyStorageKeyring::Init() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  return GnomeKeyringLoader::LoadGnomeKeyring();
}

absl::optional<std::string> KeyStorageKeyring::GetKeyImpl() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());

  absl::optional<std::string> password;
  gchar* password_c = nullptr;
  GnomeKeyringResult result =
      GnomeKeyringLoader::gnome_keyring_find_password_sync_ptr(
          &kSchema, &password_c, "application", application_name_.c_str(),
          nullptr);
  if (result == GNOME_KEYRING_RESULT_OK) {
    password = password_c;
    GnomeKeyringLoader::gnome_keyring_free_password_ptr(password_c);
  } else if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
    password = KeyStorageKeyring::AddRandomPasswordInKeyring();
  } else {
    VLOG(1) << "OSCrypt failed to use gnome-keyring";
  }

  return password;
}

absl::optional<std::string> KeyStorageKeyring::AddRandomPasswordInKeyring() {
  // Generate password
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);

  // Store generated password
  GnomeKeyringResult result =
      GnomeKeyringLoader::gnome_keyring_store_password_sync_ptr(
          &kSchema, nullptr /* default keyring */, KeyStorageLinux::kKey,
          password.c_str(), "application", application_name_.c_str(), nullptr);
  if (result != GNOME_KEYRING_RESULT_OK) {
    VLOG(1) << "OSCrypt failed to store generated password to gnome-keyring";
    return absl::nullopt;
  }

  VLOG(1) << "OSCrypt generated a new password and stored it to gnome-keyring";
  // password.value() is not empty since result == GNOME_KEYRING_RESULT_OK
  return std::move(password);
}
