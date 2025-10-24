// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/testing_value_store.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_manager.h"
#include "services/data_decoder/data_decoder_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_app_sorting.h"
#else
#include "extensions/browser/null_app_sorting.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

// A fake CWSInfoService for tests that utilize the test extension system and
// service infrastructure but do not depend on the actual functionality of the
// service.
class FakeCWSInfoService : public CWSInfoService {
 public:
  explicit FakeCWSInfoService(Profile* profile) {}

  explicit FakeCWSInfoService(const CWSInfoService&) = delete;
  FakeCWSInfoService& operator=(const CWSInfoService&) = delete;
  ~FakeCWSInfoService() override = default;

  // CWSInfoServiceInterface:
  std::optional<bool> IsLiveInCWS(const Extension& extension) const override;
  std::optional<CWSInfo> GetCWSInfo(const Extension& extension) const override;
  void CheckAndMaybeFetchInfo() override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  // KeyedService:
  // Ensure that the keyed service shutdown is a no-op.
  void Shutdown() override {}
};

std::optional<bool> FakeCWSInfoService::IsLiveInCWS(
    const Extension& extension) const {
  return true;
}

std::optional<CWSInfoServiceInterface::CWSInfo> FakeCWSInfoService::GetCWSInfo(
    const Extension& extension) const {
  return CWSInfoServiceInterface::CWSInfo();
}

std::unique_ptr<KeyedService> BuildFakeCWSService(
    content::BrowserContext* context) {
  return std::make_unique<FakeCWSInfoService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

TestExtensionSystem::InitParams::InitParams() = default;

TestExtensionSystem::InitParams::InitParams(
    base::CommandLine* command_line,
    base::FilePath install_directory,
    base::FilePath unpacked_install_directory,
    bool autoupdate_enabled,
    bool enable_extensions)
    : command_line(command_line),
      install_directory(install_directory),
      unpacked_install_directory(unpacked_install_directory),
      autoupdate_enabled(autoupdate_enabled),
      enable_extensions(enable_extensions) {}

TestExtensionSystem::InitParams::~InitParams() = default;

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      store_factory_(
          base::MakeRefCounted<value_store::TestValueStoreFactory>()),
      state_store_(std::make_unique<StateStore>(profile_,
                                                store_factory_,
                                                StateStore::BackendType::RULES,
                                                false)),
      management_policy_(std::make_unique<ManagementPolicy>()),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      app_sorting_(std::make_unique<ChromeAppSorting>(profile_)),
#else
      app_sorting_(std::make_unique<NullAppSorting>()),
#endif
      quota_service_(std::make_unique<QuotaService>()) {
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());
}

TestExtensionSystem::~TestExtensionSystem() = default;

void TestExtensionSystem::Shutdown() {
  if (extension_service_) {
    extension_service_->Shutdown();
  }

  in_process_data_decoder_.reset();
}

void TestExtensionSystem::Init() {
  Init(InitParams());
}

void TestExtensionSystem::Init(const InitParams& params) {
  base::CommandLine* command_line =
      params.command_line ? params.command_line.get()
                          : base::CommandLine::ForCurrentProcess();
  base::FilePath install_directory = params.install_directory.value_or(
      profile_->GetPath().AppendASCII(kInstallDirectoryName));
  base::FilePath unpacked_install_directory =
      params.unpacked_install_directory.value_or(
          profile_->GetPath().AppendASCII(kUnpackedInstallDirectoryName));

  CreateExtensionService(command_line, install_directory,
                         unpacked_install_directory, params.autoupdate_enabled,
                         params.enable_extensions);
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled,
    bool extensions_enabled) {
  return CreateExtensionService(command_line, install_directory,
                                base::FilePath(), autoupdate_enabled,
                                extensions_enabled);
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory,
    bool autoupdate_enabled,
    bool extensions_enabled) {
  if (CWSInfoService::Get(profile_) == nullptr) {
    Profile* profile = profile_;
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/40891982): Refactor this convenience upstream to test
    // callers. Possibly just BuiltInAppTest.BuildGuestMode.
    if (profile_->IsGuestSession()) {
      profile = profile_->GetOriginalProfile();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    // Associate a dummy CWSInfoService with this profile if necessary.
    CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&BuildFakeCWSService));
  }
  extension_service_ = std::make_unique<ExtensionService>(
      profile_, command_line, install_directory, unpacked_install_directory,
      ExtensionPrefs::Get(profile_), Blocklist::Get(profile_),
      ExtensionErrorController::Get(profile_), autoupdate_enabled,
      extensions_enabled, &ready_);

  unzip::SetUnzipperLaunchOverrideForTesting(
      base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  in_process_data_decoder_ =
      std::make_unique<data_decoder::test::InProcessDataDecoder>();

  ExternalProviderManager::Get(profile_)->ClearProvidersForTesting();
  return extension_service_.get();
}

void TestExtensionSystem::CreateUserScriptManager() {
  user_script_manager_ = std::make_unique<UserScriptManager>(profile_);
}

ExtensionService* TestExtensionSystem::extension_service() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extension_service_.get();
#else
  NOTIMPLEMENTED() << "ExtensionService is not supported on desktop android.";
  return nullptr;
#endif
}

ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

ServiceWorkerManager* TestExtensionSystem::service_worker_manager() {
  return nullptr;
}

UserScriptManager* TestExtensionSystem::user_script_manager() {
  return user_script_manager_.get();
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::dynamic_user_scripts_store() {
  return state_store_.get();
}

scoped_refptr<value_store::ValueStoreFactory>
TestExtensionSystem::store_factory() {
  return store_factory_;
}

QuotaService* TestExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* TestExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

const base::OneShotEvent& TestExtensionSystem::ready() const {
  return ready_;
}

bool TestExtensionSystem::is_ready() const {
  return ready_.is_signaled();
}

ContentVerifier* TestExtensionSystem::content_verifier() {
  return content_verifier_.get();
}

std::unique_ptr<ExtensionSet> TestExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return SharedModuleService::Get(profile_)->GetDependentExtensions(extension);
}

void TestExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

void TestExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value::Dict& attributes) {}

value_store::TestingValueStore* TestExtensionSystem::value_store() {
  // These tests use TestingValueStore in a way that ensures it only ever mints
  // instances of TestingValueStore.
  return static_cast<value_store::TestingValueStore*>(
      store_factory_->LastCreatedStore());
}

// static
std::unique_ptr<KeyedService> TestExtensionSystem::Build(
    content::BrowserContext* profile) {
  return base::WrapUnique(
      new TestExtensionSystem(static_cast<Profile*>(profile)));
}

void TestExtensionSystem::RecreateAppSorting() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_sorting_ = std::make_unique<ChromeAppSorting>(profile_);
#else
  app_sorting_ = std::make_unique<NullAppSorting>();
#endif
}

}  // namespace extensions
