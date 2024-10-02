// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_ui_view_android.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/int_string_callback.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/cxx20_erase.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/android/chrome_jni_headers/PasswordUIView_jni.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using IsInsecureCredential = CredentialEditBridge::IsInsecureCredential;

PasswordUIViewAndroid::SerializationResult SerializePasswords(
    const base::FilePath& target_directory,
    std::vector<password_manager::CredentialUIEntry> credentials) {
  // The UI should not trigger serialization if there are no passwords.
  DCHECK(!credentials.empty());

  // Creating a file will block the execution on I/O.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // Ensure that the target directory exists.
  base::File::Error error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(target_directory, &error)) {
    return {0, std::string(), base::File::ErrorToString(error)};
  }

  // Create a temporary file in the target directory to hold the serialized
  // passwords.
  base::FilePath export_file;
  if (!base::CreateTemporaryFileInDir(target_directory, &export_file)) {
    return {
        0, std::string(),
        logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())};
  }

  // Write the serialized data in CSV.
  std::string data =
      password_manager::PasswordCSVWriter::SerializePasswords(credentials);
  if (!base::WriteFile(export_file, data)) {
    return {
        0, std::string(),
        logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())};
  }

  return {static_cast<int>(credentials.size()), export_file.value(),
          std::string()};
}

}  // namespace

PasswordUIViewAndroid::PasswordUIViewAndroid(JNIEnv* env, jobject obj)
    : weak_java_ui_controller_(env, obj) {
  saved_passwords_presenter_.AddObserver(this);
  saved_passwords_presenter_.Init();
}

PasswordUIViewAndroid::~PasswordUIViewAndroid() {
  saved_passwords_presenter_.RemoveObserver(this);
}

void PasswordUIViewAndroid::Destroy(JNIEnv*, const JavaRef<jobject>&) {
  switch (state_) {
    case State::ALIVE:
      delete this;
      break;
    case State::ALIVE_SERIALIZATION_PENDING:
      // Postpone the deletion until the pending tasks are completed, so that
      // the tasks do not attempt a use after free while reading data from
      // |this|.
      state_ = State::DELETION_PENDING;
      break;
    case State::DELETION_PENDING:
      NOTREACHED();
      break;
  }
}

void PasswordUIViewAndroid::InsertPasswordEntryForTesting(
    JNIEnv* env,
    const JavaRef<jstring>& origin,
    const JavaRef<jstring>& username,
    const JavaRef<jstring>& password) {
  password_manager::PasswordForm form;
  form.url = GURL(ConvertJavaStringToUTF16(env, origin));
  form.signon_realm = password_manager::GetSignonRealm(form.url);
  form.username_value = ConvertJavaStringToUTF16(env, username);
  form.password_value = ConvertJavaStringToUTF16(env, password);

  profile_store_->AddLogin(form);
}

void PasswordUIViewAndroid::UpdatePasswordLists(JNIEnv* env,
                                                const JavaRef<jobject>&) {
  DCHECK_EQ(State::ALIVE, state_);
  UpdatePasswordLists();
}

ScopedJavaLocalRef<jobject> PasswordUIViewAndroid::GetSavedPasswordEntry(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  if ((size_t)index >= passwords_.size()) {
    return Java_PasswordUIView_createSavedPasswordEntry(
        env, ConvertUTF8ToJavaString(env, std::string()),
        ConvertUTF16ToJavaString(env, std::u16string()),
        ConvertUTF16ToJavaString(env, std::u16string()));
  }
  return Java_PasswordUIView_createSavedPasswordEntry(
      env,
      ConvertUTF8ToJavaString(
          env, password_manager::GetShownOrigin(passwords_[index])),
      ConvertUTF16ToJavaString(env, passwords_[index].username),
      ConvertUTF16ToJavaString(env, passwords_[index].password));
}

ScopedJavaLocalRef<jstring> PasswordUIViewAndroid::GetSavedPasswordException(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  if ((size_t)index >= blocked_sites_.size())
    return ConvertUTF8ToJavaString(env, std::string());
  return ConvertUTF8ToJavaString(
      env, password_manager::GetShownOrigin(blocked_sites_[index]));
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordEntry(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  if ((size_t)index >= passwords_.size())
    return;
  if (saved_passwords_presenter_.RemoveCredential(passwords_[index])) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordException(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  if ((size_t)index >= passwords_.size())
    return;
  if (saved_passwords_presenter_.RemoveCredential(passwords_[index])) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  }
}

void PasswordUIViewAndroid::HandleSerializePasswords(
    JNIEnv* env,
    const JavaRef<jobject>&,
    const JavaRef<jstring>& java_target_directory,
    const JavaRef<jobject>& success_callback,
    const JavaRef<jobject>& error_callback) {
  switch (state_) {
    case State::ALIVE:
      state_ = State::ALIVE_SERIALIZATION_PENDING;
      break;
    case State::ALIVE_SERIALIZATION_PENDING:
      // The UI should not allow the user to re-request export before finishing
      // or cancelling the pending one.
      NOTREACHED();
      return;
    case State::DELETION_PENDING:
      // The Java part should not first request destroying of |this| and then
      // ask |this| for serialized passwords.
      NOTREACHED();
      return;
  }
  std::vector<password_manager::CredentialUIEntry> credentials =
      saved_passwords_presenter_.GetSavedCredentials();
  base::EraseIf(credentials, [](const auto& credential) {
    return credential.blocked_by_user;
  });

  // The tasks are posted with base::Unretained, because deletion is postponed
  // until the reply arrives (look for the occurrences of DELETION_PENDING in
  // this file). The background processing is not expected to take very long,
  // but still long enough not to block the UI thread. The main concern here is
  // not to avoid the background computation if |this| is about to be deleted
  // but to simply avoid use after free from the background task runner.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          &SerializePasswords,
          base::FilePath(ConvertJavaStringToUTF8(env, java_target_directory)),
          std::move(credentials)),
      base::BindOnce(&PasswordUIViewAndroid::PostSerializedPasswords,
                     base::Unretained(this),
                     base::android::ScopedJavaGlobalRef<jobject>(
                         env, success_callback.obj()),
                     base::android::ScopedJavaGlobalRef<jobject>(
                         env, error_callback.obj())));
}

void PasswordUIViewAndroid::HandleShowPasswordEntryEditingView(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& context,
    const base::android::JavaRef<jobject>& settings_launcher,
    int index,
    const JavaParamRef<jobject>& obj) {
  if ((size_t)index >= passwords_.size() || credential_edit_bridge_) {
    return;
  }
  bool is_using_account_store = passwords_[index].stored_in.contains(
      password_manager::PasswordForm::Store::kAccountStore);
  credential_edit_bridge_ = CredentialEditBridge::MaybeCreate(
      passwords_[index], IsInsecureCredential(false),
      GetUsernamesForRealm(saved_passwords_presenter_.GetSavedCredentials(),
                           passwords_[index].GetFirstSignonRealm(),
                           is_using_account_store),
      &saved_passwords_presenter_,
      base::BindOnce(&PasswordUIViewAndroid::OnEditUIDismissed,
                     base::Unretained(this)),
      context, settings_launcher);
}

void PasswordUIViewAndroid::HandleShowBlockedCredentialView(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& context,
    const base::android::JavaRef<jobject>& settings_launcher,
    int index,
    const JavaParamRef<jobject>& obj) {
  if ((size_t)index >= blocked_sites_.size() || credential_edit_bridge_) {
    return;
  }
  credential_edit_bridge_ = CredentialEditBridge::MaybeCreate(
      blocked_sites_[index], IsInsecureCredential(false),
      std::vector<std::u16string>(), &saved_passwords_presenter_,
      base::BindOnce(&PasswordUIViewAndroid::OnEditUIDismissed,
                     base::Unretained(this)),
      context, settings_launcher);
}

void PasswordUIViewAndroid::OnEditUIDismissed() {
  credential_edit_bridge_.reset();
}

ScopedJavaLocalRef<jstring> JNI_PasswordUIView_GetAccountDashboardURL(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, l10n_util::GetStringUTF16(IDS_PASSWORDS_WEB_LINK));
}

ScopedJavaLocalRef<jstring> JNI_PasswordUIView_GetTrustedVaultLearnMoreURL(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, chrome::kSyncTrustedVaultLearnMoreURL);
}

jboolean JNI_PasswordUIView_HasAccountForLeakCheckRequest(JNIEnv* env) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  return password_manager::LeakDetectionCheckImpl::HasAccountForRequest(
      identity_manager);
}

// static
static jlong JNI_PasswordUIView_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  PasswordUIViewAndroid* controller = new PasswordUIViewAndroid(env, obj.obj());
  return reinterpret_cast<intptr_t>(controller);
}

void PasswordUIViewAndroid::OnSavedPasswordsChanged() {
  UpdatePasswordLists();
}

void PasswordUIViewAndroid::UpdatePasswordLists() {
  passwords_.clear();
  blocked_sites_.clear();
  for (auto& credential : saved_passwords_presenter_.GetSavedCredentials()) {
    if (credential.blocked_by_user) {
      blocked_sites_.push_back(std::move(credential));
    } else {
      passwords_.push_back(std::move(credential));
    }
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordListAvailable(
        env, ui_controller, static_cast<int>(passwords_.size()));
    Java_PasswordUIView_passwordExceptionListAvailable(
        env, ui_controller, static_cast<int>(blocked_sites_.size()));
  }
}

void PasswordUIViewAndroid::PostSerializedPasswords(
    const base::android::JavaRef<jobject>& success_callback,
    const base::android::JavaRef<jobject>& error_callback,
    SerializationResult serialization_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (state_) {
    case State::ALIVE:
      NOTREACHED();
      break;
    case State::ALIVE_SERIALIZATION_PENDING: {
      state_ = State::ALIVE;
      if (export_target_for_testing_) {
        *export_target_for_testing_ = serialization_result;
      } else {
        if (serialization_result.entries_count) {
          base::android::RunIntStringCallbackAndroid(
              success_callback, serialization_result.entries_count,
              serialization_result.exported_file_path);
        } else {
          base::android::RunStringCallbackAndroid(error_callback,
                                                  serialization_result.error);
        }
      }
      break;
    }
    case State::DELETION_PENDING:
      delete this;
      break;
  }
}
